//
// Created by BraxtonN on 2/11/2018.
//

#include "GarbageCollector.h"
#include "../oo/Object.h"

static GarbageCollector *self = NULL;

void* __malloc(size_t bytes)
{
    void* ptr =NULL;
    bool gc=false;
    alloc_bytes:
    ptr=malloc(bytes);

    if(GarbageCollector::self != NULL && ptr == NULL) {
        if(gc) {
            throw Exception("out of memory");
        } else {
            gc = true;
            GarbageCollector::self->collect(GC_LOW);
            goto alloc_bytes;
        }
    } else {
        return ptr;
    }
}
void* __calloc(size_t n, size_t bytes)
{
    void* ptr =NULL;
    bool gc=false;
    alloc_bytes:
    ptr=calloc(n, bytes);

    if(ptr == NULL) {
        if(gc) {
            throw Exception("out of memory");
        } else {
            gc=true;
            GarbageCollector::self->collect(GC_LOW);
            goto alloc_bytes;
        }
    } else {
        return ptr;
    }
}
void* __realloc(void *ptr, size_t bytes)
{
    void* rmap =NULL;
    bool gc=false;
    alloc_bytes:
    rmap=realloc(ptr, bytes);

    if(rmap == NULL) {
        if(gc) {
            throw Exception("out of memory");
        } else {
            gc=true;
            GarbageCollector::self->collect(GC_LOW);
            goto alloc_bytes;
        }
    } else {
        return rmap;
    }
}

void GarbageCollector::initilize() {
    self=(GarbageCollector*)malloc(sizeof(GarbageCollector)*1);
    self->mutex = Mutex();
    self->heap.init();
    self->managedBytes=0;
    self->adultObjects=0;
    self->youngObjects=0;
    self->oldObjects=0;
    self->yObjs=0;
    self->aObjs=0;
    self->oObjs=0;
}

void GarbageCollector::freeObject(Object *object) {
    if(object != NULL && object->object != NULL)
    {
        object->object->mutex.acquire(INDEFINITE);
        object->object->refCount--;

        switch(object->object->generation) {
            case gc_young:
                yObjs++;
                break;
            case gc_adult:
                aObjs++;
                break;
            case gc_old:
                oObjs++;
                break;
        }

        object->object->mutex.release();
        object->object = NULL;
    }
}

void GarbageCollector::shutdown() {
    if(self != NULL) {
        managedBytes=0;
        /* Clear out all memory */
        for(unsigned int i = 0; i < heap.size(); i++) {
            collect(heap.get(i));
        }
        heap.free();
        std::free(self); self = NULL;
    }
}

void GarbageCollector::collect(CollectionPolicy policy) {
    mutex.acquire(INDEFINITE);

    if(policy == GC_LOW) {
        Thread::suspendAllThreads();

        /**
         * To attempt to approve a large memory request we want to take the
         * worst case scenareo route to try to fuffil the memory request. To
         * avoid lags in the application big memory requests should not be
         * performed often
         */
        collectYoungObjects();
        collectAdultObjects();
        collectOldObjects();

        Thread::resumeAllThreads();
    } else if(policy == GC_EXPLICIT) {
        /**
         * Force collection of both generations
         * We only do the first 2 generations because we want
         * to prevent lag when freeing up memory. the Old generation will
         * always be the largest generation
         */
            collectYoungObjects();
            collectAdultObjects();
    } else if(policy == GC_CONCURRENT) {
        /**
         * This should only be called by the GC thread itsself
         */
        if(GC_COLLECT_YOUNG()) {
            collectYoungObjects();
        }
        if(GC_COLLECT_ADULT()) {
            collectAdultObjects();
        }
        if(GC_COLLECT_OLD()) {
            collectOldObjects();
        }
    }

    mutex.release();
}

void GarbageCollector::collectYoungObjects() {
    SharpObject *object;
    yObjs = 0;

    reset:
    for(unsigned int i = 0; i < heap.size(); i++) {
        object = heap.get(i);

        if(object->generation == gc_young) {
            /* If all threads are suspended we don't have to worry about any interference */
            if(!Thread::allThreadsSuspended())
                object->mutex.acquire(INDEFINITE);

            // free object
            if(object->refCount == 0) {
                collect(object);
                youngObjects--;
                heap.remove(i); // drop pointer and reset list
                goto reset;
            } else {
                adultObjects++;

                object->generation = gc_adult;
                if(!Thread::allThreadsSuspended())
                    object->mutex.release();
            }

        }
    }
}

void GarbageCollector::collectAdultObjects() {
    SharpObject *object;
    aObjs = 0;

    reset:
    for(unsigned int i = 0; i < heap.size(); i++) {
        object = heap.get(i);

        if(object->generation == gc_adult) {
            /* If all threads are suspended we don't have to worry about any interference */
            if(!Thread::allThreadsSuspended())
                object->mutex.acquire(INDEFINITE);

            // free object
            if(object->refCount == 0) {
                collect(object);
                adultObjects--;
                heap.remove(i); // drop pointer and reset list
                goto reset;
            } else {
                oldObjects++;

                object->generation = gc_old;
                if(!Thread::allThreadsSuspended())
                    object->mutex.release();
            }

        }
    }
}

void GarbageCollector::collectOldObjects() {
    SharpObject *object;
    oObjs = 0;

    reset:
    for(unsigned int i = 0; i < heap.size(); i++) {
        object = heap.get(i);

        if(object->generation == gc_old) {
            /* If all threads are suspended we don't have to worry about any interference */
            if(!Thread::allThreadsSuspended())
                object->mutex.acquire(INDEFINITE);

            // free object
            if(object->refCount == 0) {
                collect(object);
                oldObjects--;
                heap.remove(i); // drop pointer and reset list
                goto reset;
            } else {
                /**
                 * We are already at the highest generation so we just skip this
                 * object
                 */
                if(!Thread::allThreadsSuspended())
                    object->mutex.release();
            }

        }
    }
}

void GarbageCollector::run() {
    const unsigned int sMaxRetries = 128 * GC_SPIN_MULTIPLIER;
    unsigned int retryCount = 0;

    for(;;) {
        if(thread_self->suspendPending)
            Thread::suspendSelf();
        if(thread_self->state == THREAD_KILLED)
            return;

        if(!messageQueue.empty()) {
            mutex.acquire(INDEFINITE);
            CollectionPolicy policy = messageQueue.last();
            messageQueue.pop_back();
            mutex.release();

            collect(policy);
        }

        if (retryCount++ == sMaxRetries)
        {
            retryCount = 0;
#ifdef WIN32_
            Sleep(GC_SLEEP_INTERVAL);
#endif
#ifdef POSIX_
            usleep(GC_SLEEP_INTERVAL*POSIX_USEC_INTERVAL);
#endif
        } else {
            /**
             * Attempt to collect objects based on the appropriate
             * conditions. This call does not garuntee that any collections
             * will happen
             */
            collect(GC_CONCURRENT);
        }

    }
}

#ifdef WIN32_
DWORD WINAPI
#endif
#ifdef POSIX_
void*
#endif
GarbageCollector::threadStart(void *pVoid) {
    thread_self =(Thread*)pVoid;
    thread_self->state = THREAD_RUNNING;

    try {
        self->run();
    } catch(Exception &e){
        /* Should never happen */
        thread_self->exceptionThrown =true;
        thread_self->throwable=e.getThrowable();
    }

        /*
         * Check for uncaught exception in thread before exit
         */
    thread_self->exit();
    cout << "gc exited" << endl;
#ifdef WIN32_
    return 0;
#endif
#ifdef POSIX_
    return NULL;
#endif
}

void GarbageCollector::sendMessage(CollectionPolicy message) {
    mutex.acquire(INDEFINITE);
    messageQueue.push_back(message);
    mutex.release();
}
