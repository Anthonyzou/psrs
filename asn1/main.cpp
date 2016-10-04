// Anthony Ou
// 1248175
// cmput 481
// Oct 5, 2016
#include "main.h"

using namespace std;

long phase3 = 0;
typedef chrono::microseconds time_u;

inline void handleChunk(int idx, vec results){
    unique_lock<mutex> lk(p4[idx]);
    phase4[idx].insert(phase4[idx].end(), results.begin(), results.end());
    threadsDone[idx]++;
    p4CV[idx].notify_one();
}

void worker(const int id, promise<vec> prom, const int from, const int end){
    //PHASE 1
    sort(&randomArr[from], &randomArr[end]);
    vec phase1Arr;
    for(auto i = from; i < end; i += sampleIntervals){
        phase1Arr.push_back(randomArr[i]);
    }
    prom.set_value(phase1Arr);

    //PHASE 3
    auto begin =  chrono::steady_clock::now();
    auto idx = 0;
    auto pivots = phase2Vector.get();
    auto pivot = pivots[idx];
    vec results;
    for(auto i = from; i < end; i++){
        auto k = randomArr[i];
        if(pivot < k && pivots.size() > idx){
            handleChunk(idx, results);
            results.clear();
            ++idx;
            if(idx < pivots.size())
                pivot = pivots[idx];
        }
        results.push_back(k);
    }
    handleChunk(idx, results);

    phase3 = chrono::duration_cast<time_u>(chrono::steady_clock::now() - begin).count();

    unique_lock<mutex> lk(p4[id]);
    p4CV[id].wait(lk, [id](){ return threadsDone[id] == PROCESSORS; });

    //PHASE 4 SORT
    merge_sort(phase4[id].begin(), phase4[id].end());
}

int main(int argc, char ** argv) {
    init(argc, argv);

    vector<thread> threads;
    vector<future<vec>> phase1Results;
    vec results, subResults, phase4Results;

//    cout << "NUM processors " << thread::hardware_concurrency() << endl << "totalElements " << totalElements << endl;

    auto begin = chrono::steady_clock::now();
    //CREATE THREADS
    for(int i = 0; i < PROCESSORS; i++) {
        promise<vec> phase1Prom;
        auto f = phase1Prom.get_future();
        phase1Results.push_back(move(f));
        threads.push_back(thread(&worker, i, move(phase1Prom), i * numElements, (i + 1) * numElements));
    }

    //PHASE 2
    for(auto &result: phase1Results){
        auto subArrays = result.get();
        results.insert(results.end(), subArrays.begin(), subArrays.end());
    }
    cout << chrono::duration_cast<time_u>(chrono::steady_clock::now() - begin).count() << " ";

    auto PHASE2START = chrono::steady_clock::now();
    sort(results.begin(), results.end());

    //PHASE 2 GET PIVOT POINTS
    auto k = 0;
    for(auto i = PROCESSORS; k++ < PROCESSORS-1; i += PROCESSORS)
        subResults.push_back(results[i]);

    phase2Promise.set_value(subResults);
    cout << chrono::duration_cast<time_u>(chrono::steady_clock::now() - PHASE2START).count() << " ";

    auto PHASE3START = chrono::steady_clock::now();
    //END PHASE 4
    for(auto &it : threads) it.join();
    auto end = chrono::steady_clock::now();
    cout << phase3/PROCESSORS << " ";
    cout << chrono::duration_cast<time_u>(chrono::steady_clock::now() - PHASE3START).count() << " ";

    //COMBINE RESULTS FROM PHASE 4
    for(auto &id : phase4)
        phase4Results.insert(phase4Results.end(), id.begin(), id.end());

    cout << chrono::duration_cast<time_u>(end - begin).count() <<endl;
//    cout << numElements <<" "<< totalElements <<" "<< PROCESSORS <<" "<< seed;
    return ((is_sorted(phase4Results.begin(), phase4Results.end()) == 1) ? 0 : 1);
}
