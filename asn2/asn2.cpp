// Anthony Ou
// CMPUT 481
// Nov 2016 - asn 2

#include "asn2.h"

namespace mpi = boost::mpi;
using namespace mpi;
using namespace boost;
using namespace std;
using namespace chrono;

void phase1(const int from, const int end, communicator world, vector<vec> &phase1Results) {
    vec phase1;
    sort(randomArr.begin() + from, randomArr.begin() + end);
    for (auto i = randomArr.begin() + from; i < randomArr.begin() + end; i += sampleIntervals)
        phase1.push_back(*i);

    if (world.rank() == 0)
        gather(world, phase1, phase1Results, 0);
    else
        gather(world, phase1, 0);
}

void phase2(const communicator world, vector<vec> &phase1Results, vec &pivots) {
    if (world.rank() != 0) return;
    vec temp;
    // PHASE2
    for (auto &proc : phase1Results)
        sortedMerge(temp, proc);

    const int p = (int) floor(world.size()/2);
    for (auto i = world.size()+p, k = 1; k++ < world.size(); i += world.size())
        pivots.push_back(temp[i]);
}

void phase3(const int from, const int end, const communicator world, vec &pivots, vector<request> &requests) {
    // Recieve a broadcast of the pivots
    broadcast(world, pivots, 0);

    // Build partitions and send them to the other processes
    auto idx = 0;
    auto movingIt = randomArr.begin() + from;
    auto endPoint = randomArr.begin() + end;
    for (auto &pivot : pivots) {
        auto nextPoint = partition(movingIt, endPoint, [pivot](vecType em) { return em <= pivot; });
        vec tmp(movingIt, nextPoint);
        requests.push_back(world.isend(idx, idx, tmp));
        movingIt = nextPoint;
        idx++;
    }

    requests.push_back(world.isend(idx, idx, vec(movingIt, endPoint)));
}

void phase4(const communicator world, vec &finalResults, vector<request> & requests) {
    vec temp, result;
    // recieve the partitions and then concat them.
    for (auto messages = 0; messages < world.size(); messages++) {
        world.recv(any_source, world.rank(), temp);
        sortedMerge(result, temp);
    }
    wait_all(requests.begin(), requests.end());
    request r = world.isend(0, world.rank(), result);
    if (world.rank() == 0) {
        for (int i = 0; i < world.size(); i++){
            world.recv(i, i, temp);
            finalResults.insert(finalResults.end(), temp.begin(), temp.end());
        }
    }
    r.wait();
}

int main(int argc, char **argv) {
    init(argc, argv);
    environment env(argc, argv, true);
    communicator world;

    if (world.rank() == 0)
        randomArr = randomArray(totalElements);
//        randomArr = {16,2,17,24,33,28,30,1,0,27,9,25,34,23,19,18,11,7, 21,13,8,35,12,29,6,3,4,14,22,15,32,10,26,31,20,5};

    // Give the array to everyone
    // Rank 0 will send the random array above. Other processes will recieve the value and
    // assign it to the variable randomArr.
    broadcast(world, randomArr, 0);

    // KEEP THIS BELOW THE BROADCAST
    perProcess = randomArr.size() / world.size();
    sampleIntervals = floor(randomArr.size() / (world.size() * world.size()));

    const int from = world.rank() * perProcess;
    // last process gets the rest of the elements
    const int end = world.rank() == world.size() - 1 ? randomArr.size() : from + perProcess;

    auto start = steady_clock::now();
    std::chrono::steady_clock::time_point timer;

    // PHASE 1
    vector<vec> phase1Results;
    if (world.rank() == 0) timer = steady_clock::now();
    phase1(from, end, world, phase1Results);
    if (world.rank() == 0) cout << duration_cast<time_u>(chrono::steady_clock::now() - timer).count() << ",";

    // PHASE 2
    vec pivots, temp;
    if (world.rank() == 0) timer = steady_clock::now();
    phase2(world, phase1Results, pivots);
    if (world.rank() == 0) cout << duration_cast<time_u>(chrono::steady_clock::now() - timer).count() << ",";

    // PHASE 3
    vec result, finalResults;
    vector<request> requests;
    if (world.rank() == 0) timer = steady_clock::now();
    phase3(from, end, world, pivots, requests);
    if (world.rank() == 0) cout << duration_cast<time_u>(chrono::steady_clock::now() - timer).count() << ",";

    // PHASE 4
    if (world.rank() == 0) timer = steady_clock::now();
    phase4(world, finalResults, requests);
    if (world.rank() == 0) cout << duration_cast<time_u>(chrono::steady_clock::now() - timer).count() << ",";

    auto endTime = steady_clock::now();
    if (world.rank() == 0) {
        auto sorted = is_sorted(finalResults.begin(), finalResults.end());
        auto correct = sorted && randomArr.size() == finalResults.size();
        cout << (correct ? duration_cast<time_u>(endTime - start).count() : -1) << endl;
        assert(correct);
        return correct ^ 1;
    } else
        return 0;
}
