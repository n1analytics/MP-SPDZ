/*
 * Coordinator.h
 *
 */

#ifndef TOOLS_COORDINATOR_H_
#define TOOLS_COORDINATOR_H_

#include "Networking/DotsPlayer.h"
#include "Signal.h"
#include "Lock.h"

class Coordinator
{
    DotsPlayer P;

    WaitQueue<string> in;

    pthread_t thread;

    map<string, int> waiting;

    map<string, WaitQueue<int>> signals;

    Lock lock;

    WaitQueue<int> done;

    double waited;

    static void* run_thread(void* coordinator);

public:
    Coordinator(const Names& N, string type_name);
    ~Coordinator();

    void run();

    void wait(const string& id);
    void finished();
};

#endif /* TOOLS_COORDINATOR_H_ */
