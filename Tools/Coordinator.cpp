/*
 * Coordinator.cpp
 *
 */

#include "Coordinator.h"
#include "Bundle.h"
#include "Processor/OnlineOptions.h"

void* Coordinator::run_thread(void* coordinator)
{
    ((Coordinator*) coordinator)->run();
    return 0;
}

Coordinator::Coordinator(const Names& N __attribute__((unused)), string type_name) :
        P("coordinate-" + type_name), waited(0)
{
    pthread_create(&thread, 0, run_thread, this);
}

Coordinator::~Coordinator()
{
    in.stop();
    pthread_join(thread, 0);
    if (waited != 0 and OnlineOptions::singleton.verbose)
        cerr << "Coordination took " << waited << " seconds" << endl;
}

void Coordinator::run()
{
    string id;
    while (in.pop(id))
    {
        Bundle<octetStream> bundle(P);
        bundle.mine = id;
        P.unchecked_broadcast(bundle);
        for (auto& x : bundle)
            waiting[string((char*) x.get_data(), x.get_length())]++;
        for (auto& x : waiting)
        {
#ifdef DEBUG_COORD
            cout << x.first << " at " << x.second << endl;
#endif
            if (x.second == P.num_players())
            {
                lock.lock();
                auto& signal = signals[x.first];
                lock.unlock();
                signal.push(0);
                done.pop();
                x.second = 0;
            }
        }
    }
}

void Coordinator::wait(const string& id)
{
    lock.lock();
    auto& signal = signals[id];
    lock.unlock();
    in.push(id);
#ifdef DEBUG_COORD
    cerr << id << " waits" << endl;
#endif
    RunningTimer timer;
    signal.pop();
#ifdef DEBUG_COORD
    cerr << id << " good to go after " << timer.elapsed() << endl;
#endif
    waited += timer.elapsed();
}

void Coordinator::finished()
{
    done.push(0);
}
