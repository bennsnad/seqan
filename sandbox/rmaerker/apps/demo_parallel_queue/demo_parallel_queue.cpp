// ==========================================================================
//                            demo_parallel_queue
// ==========================================================================
// Copyright (c) 2006-2013, Knut Reinert, FU Berlin
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of Knut Reinert or the FU Berlin nor the names of
//       its contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL KNUT REINERT OR THE FU BERLIN BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
// DAMAGE.
//
// ==========================================================================
// Author: Rene Rahn <rene.rahn@fu-berlin.de>
// ==========================================================================

#ifdef SEQAN_ENABLE_DEBUG
#undef SEQAN_ENABLE_DEBUG
#endif
#define SEQAN_ENABLE_DEBUG 0

#include <string>
#include <vector>
#include <sstream>
#include <pthread.h>
#include <stdio.h>

#include <seqan/basic.h>
#include <seqan/sequence.h>
#include <seqan/parallel.h>
#include <seqan/random.h>
#include <seqan/stream.h>

#include <seqan/system/system_thread.h>

#define NUM_THREADS     10

using namespace seqan;

// ==========================================================================
// Classes
// ==========================================================================

template <typename TVariantMap>
class MergePointMap_
{
public:
    typedef unsigned TValue;
    typedef String<bool, Packed<> > TDeltaCoverage;
    typedef String<TValue> TMergePoints;

    TVariantMap*            _varMapPtr;
//    TDeltaCoverage
    unsigned _mergeCoverage;
    String<unsigned>    _mergePoints;  // Stores the reference position of the merge point

    MergePointMap_() : _varMapPtr(NULL), _mergeCoverage(), _mergePoints()
    {}

    MergePointMap_(TVariantMap & map) : _varMapPtr(&map), _mergeCoverage(), _mergePoints()
    {
//        resize(_mergeCoverage, length(map), false, Exact());
        _mergeCoverage = 2;
        resize(_mergePoints, 1);
//        resize(_mergePoints, length(map), TValue(2));
    }

    // Copy constructor.
//    MergePointMap_(MergePointMap_ const & other)
//    {
//        _copy(*this, other);
//    }
//
//    // Assignment Operator.
//    MergePointMap_ & operator=(MergePointMap_ const & other)
//    {
//        if (this != &other)
//        {
//
//        }
//
//        return *this;
//    }
};

struct ArtificialBeak
{
    double breakTime;

    ArtificialBeak() : breakTime(1)
    {}

    ArtificialBeak(double time) : breakTime(time)
    {}

    ArtificialBeak(ArtificialBeak const & other) : breakTime(other.breakTime)
    {}

    inline void
    execBreak(CharString const & message)
    {
        // Stops the process waiting for the given number of seconds.
        double startWatch = sysTime();
        SEQAN_OMP_PRAGMA(critical(cout))
        {
            std::cout << message << std::flush;
        }

        while ((sysTime() - startWatch) < breakTime);


        // We could add a print message here.
    }
};


struct BreakPointList
{
    typedef Pair<unsigned, ArtificialBeak> TPair;


    String<TPair> breakPointString;

    static const int LENGTH = 50;
    BreakPointList()
    {
        Rng<MersenneTwister> rng(43);
        Pdf<Uniform<unsigned> > pdf(0, 100);
        Pdf<Uniform<double> > pdfTime(0.5, 2);
        resize(breakPointString, LENGTH, Exact());
        String<unsigned> posVec;
        resize(posVec, LENGTH, Exact());
        for (unsigned i = 0; i < LENGTH; ++i)
            posVec[i] =  pickRandomNumber(rng, pdf);

        std::sort(begin(posVec, Standard()), end(posVec, Standard()));
        for (unsigned i = 0; i < LENGTH; ++i)
            breakPointString[i] = TPair(posVec[i], ArtificialBeak(pickRandomNumber(rng, pdfTime)));
    }

};

struct WorkerFunctor
{
    ArtificialBeak _break;

    WorkerFunctor(ArtificialBeak artBreak) : _break(artBreak)
    {}

    inline void operator()()
    {
        _break.execBreak("\nDo busy work\n");
    }
};

// ==========================================================================
// Functions
// ==========================================================================
//
//template <typename TWorker>
//inline void _doWork(TWorker & worker)
//{
////        std::cout << "DECREASE: " << length(queue) << std::endl;
//    CharString buff;
//    lexicalCast2(buff, omp_get_thread_num());
//    insert(buff, 0, "Thread ");
//    append(buff, ": Let me do this!\n");
//    worker.execBreak(buff);
//}
//
//
//template <typename TQueue, typename TList>
//inline void _master(TQueue & queue, TList const & breakPointList)
//{
//    typedef Pair<unsigned, ArtificialBeak> TPair;
//    ArtificialBeak shortBreak(0.05);
//    ArtificialBeak masterWorker;
//    unsigned queueSize = ((omp_get_num_threads() - 1) * 4) + 1;
//    unsigned lastPos = 0;
//    for (unsigned i = 0; i < length(breakPointList.breakPointString); ++i)
//    {
//            // The Master thread. -> Producer.
//        TPair pair = breakPointList.breakPointString[i];
//        for (unsigned j = lastPos; j < pair.i1; ++j)
//        {
//            shortBreak.execBreak(".");
//        }
//        appendValue(queue, pair.i2, Generous());
//
//        while(length(queue) > queueSize)
//        {
//            if (tryPopFront(masterWorker, queue, Parallel()))
//                _doWork(masterWorker);
//        }
//
////        std::cout << "APPEND: " << length(queue) << std::endl;
//        lastPos = pair.i1;
//    }
//}
//
//template <typename TQueue, typename TWorkerArrays>
//inline void _workerIdle(TQueue & queue, TWorkerArrays & workers)
//{
//    printf("Thread %i: I slept well and are ready to work.\n", omp_get_thread_num());
//
//    while(popFront(workers[omp_get_thread_num()], queue))
//        _doWork(workers[omp_get_thread_num()]);
//}
//
//inline void
//traverse(BreakPointList const & breakPointList)
//{
//    typedef ConcurrentQueue<ArtificialBeak> TQueue;
//    TQueue queue;
//
//    ArtificialBeak workerBreak;
//    String<ArtificialBeak> workerArray;
//    resize(workerArray, omp_get_num_threads(), Exact());
//
//    SEQAN_OMP_PRAGMA(parallel)
//    {
//        // Single Producer -> master.
//        SEQAN_OMP_PRAGMA(master)
//        {
//            ScopedWriteLock<TQueue> writeLock(queue);
//
//            waitForWriters(queue, 1);  // Barrier for writers to set up.
//
//            _master(queue, breakPointList);
//        }  // At end of scope all locked writers are unlocked.
//
//        ScopedReadLock<TQueue> readLock(queue);
//        waitForFirstValue(queue);  // Barrier for reader to wait for writer.
//
//        SEQAN_OMP_PRAGMA(critical(cout))
//        {
//            printf("Thread %i: I slept well and are ready to work.\n", omp_get_thread_num());
//        }
//
//        while(popFront(workerArray[omp_get_thread_num()], queue))
//            _doWork(workerArray[omp_get_thread_num()]);
//
//        SEQAN_OMP_PRAGMA(critical(cout))
//        {
//            printf("Thread %i: I am tired and go home.\n", omp_get_thread_num());
//        }
//    }
//
//    if (!empty(queue))
//        std::cerr << "Back to work! There is still some' to do!!!" << std::endl;
//    else
//        std::cerr << "Great job! Enjoy your evening!" << std::endl;
////    unsigned lastPos = 0;
////
////
////        // Now we produce work and let it consume from some one.
////
////        {// Invoke parallel Worker
////            Thread<WorkerFunctor> thread(pair.i2);
////            thread.open();
////        }
////        lastPos = pair.i1;
////    }
//}
//
//void *PrintHello(void *threadid)
//{
//   long tid;
//   tid = (long)threadid;
//   printf("Hello World! It's me, thread #%ld!\n", tid);
//   pthread_exit(NULL);
//}
//
//struct PrintHelloFunctor
//{
//    long threadId;
//
//    PrintHelloFunctor()
//    {}
//
//    PrintHelloFunctor(long id) : threadId(id)
//    {}
//
//    void operator()()
//    {
//        if (threadId % 2 == 0)
//        {
//            ArtificialBeak breaker(1);
//            breaker.execBreak("\nWho is waiting?\n");
//        }
//        printf("Hello World! It's me, thread #%ld!\n", threadId);
//    }
//};

inline void _function()
{
    typedef String<char> TTest;
//    typedef unsigned TTest;
    typedef MergePointMap_<TTest> TMap;
    typedef ConcurrentQueue<TTest> TQueue;

    TTest test = "This is a simple test!";
    TQueue queue;

    SEQAN_OMP_PRAGMA(parallel)
    {

        if (omp_get_thread_num() == 0)
        {
            seqan::ScopedWriteLock<TQueue> writeLock(queue);

            waitForWriters(queue, 1);
            for (unsigned i = 0; i < 5; ++i)
            {
                std::stringstream tmpStr;
                tmpStr << i;
                TTest copy = test;
                append(copy, tmpStr.str());
                appendValue(queue, copy);
            }
        }

        seqan::ScopedReadLock<TQueue> readLock(queue);
        waitForFirstValue(queue);
        TTest threadLocalMap;
        while (popFront(threadLocalMap, queue, Parallel()))
        {
            SEQAN_OMP_PRAGMA(critical(cout))
            {
                printf("Thread %i is working!\n", omp_get_thread_num());
            }
        }
    }
}
// --------------------------------------------------------------------------
// Function main()
// --------------------------------------------------------------------------

int main(int argc, char** argv)
{
//    double progStart = sysTime();
    unsigned numThreads = 1;
    if (argc > 1)
    {
        std::stringstream str;
        str << argv[1];
        str >> numThreads;
    }

//    omp_set_num_threads(numThreads);
//    BreakPointList list;

//    traverse(list);
    _function();

//    std::cout << "Time needed: " << sysTime() - progStart << " s" << std::endl;
    return 0;

//    typedef Thread<PrintHelloFunctor> TThread;
//
//    TThread threadArray[NUM_THREADS];
//
//    int rc;
//    long t;
//    for(t=0; t<NUM_THREADS; t++){
//       printf("In main: creating thread %ld\n", t);
//       threadArray[t].worker.threadId = t;
//
//       rc = run(threadArray[t]);
//       if (!rc){
//          printf("ERROR; return code from pthread_create() is %d\n", rc);
//          exit(-1);
//       }
//    }
//
//    /* Last thing that main() should do */
//    pthread_exit(NULL);
}
