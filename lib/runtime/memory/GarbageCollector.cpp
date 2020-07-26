//
// Created by BraxtonN on 2/11/2018.
//

#include <random>
#include "GarbageCollector.h"
#include "../symbols/Object.h"
#include "../Thread.h"
#include "../symbols/Field.h"
#include "../../util/time.h"
#include "../../util/KeyPair.h"
#include "../Exe.h"
#include "../Manifest.h"
#include "../VirtualMachine.h"
#include <thread>
#include <algorithm>

uInt hbytes = 0;
GarbageCollector *GarbageCollector::self = nullptr;

const Int MEMORY_POOL_SAMPLE_SIZE = 10;
const Int MAX_DOWNGRADE_ATTEMPTS = 3;
uInt memoryPoolResults[MEMORY_POOL_SAMPLE_SIZE];
Int samplesReceived =0, downgradeAttempts = 0;

void* __malloc(uInt bytes)
{
    void* ptr =nullptr;
    bool gc=false;
    alloc_bytes:
    if(GarbageCollector::self != nullptr && !GarbageCollector::self->spaceAvailable(bytes))
        goto lowmem;
    ptr=malloc(bytes);

    if(GarbageCollector::self != nullptr && ptr == nullptr) {
        if(gc) {
            lowmem:
            throw Exception(vm.OutOfMemoryExcept, "out of memory");
        } else {
            gc=true;
            if(vm.state == VM_RUNNING && GarbageCollector::self != nullptr) {
                GarbageCollector::self->collect(GC_LOW);
                goto alloc_bytes;
            } else
                throw Exception(vm.OutOfMemoryExcept, "out of memory");
        }
    } else {
        return ptr;
    }
}
void* __calloc(uInt n, uInt bytes)
{
    void* ptr =nullptr;
    bool gc=false;
    alloc_bytes:
    if(GarbageCollector::self != nullptr && !GarbageCollector::self->spaceAvailable(n*bytes))
        goto lowmem;
    ptr=calloc(n, bytes);

    if(ptr == nullptr) {
        if(gc) {
            lowmem:
            throw Exception(vm.OutOfMemoryExcept, "out of memory");
        } else {
            gc=true;
            if(vm.state == VM_RUNNING && GarbageCollector::self != nullptr) {
                GarbageCollector::self->collect(GC_LOW);
                goto alloc_bytes;
            } else
                throw Exception(vm.OutOfMemoryExcept, "out of memory");
        }
    } else {
        return ptr;
    }
}
void* __realloc(void *ptr, uInt bytes, uInt old)
{
    void* rmap =nullptr;
    bool gc=false;
    alloc_bytes:
    if(GarbageCollector::self != nullptr && !GarbageCollector::self->spaceAvailable(bytes-old))
        goto lowmem;
    rmap=realloc(ptr, bytes);

    if(rmap == nullptr) {
        if(gc) {
            lowmem:
            throw Exception(vm.OutOfMemoryExcept, "out of memory");
        } else {
            gc=true;
            if(vm.state == VM_RUNNING && GarbageCollector::self != nullptr) {
                GarbageCollector::self->collect(GC_LOW);
                goto alloc_bytes;
            } else
                throw Exception(vm.OutOfMemoryExcept, "out of memory");
        }
    } else {
        return rmap;
    }
}

void GarbageCollector::initilize() {
    self=(GarbageCollector*)malloc(sizeof(GarbageCollector));

    self->_Mheap = (SharpObject*)malloc(sizeof(SharpObject)); // HEAD
    if(self->_Mheap==NULL)
        throw Exception(vm.OutOfMemoryExcept, "out of memory");
    new (&self->mutex) std::recursive_mutex();
    self->_Mheap->init(0, _stype_none);
    self->tail = self->_Mheap; // set tail to self for later use
    new (&self->heapSize) std::atomic<uInt>();
    new (&self->managedBytes) std::atomic<uInt>();
    new (&self->memoryLimit) std::atomic<uInt>();
    new (&self->adultObjects) std::atomic<uInt>();
    new (&self->youngObjects) std::atomic<uInt>();
    new (&self->oldObjects) std::atomic<uInt>();
    new (&self->yObjs) std::atomic<uInt>();
    new (&self->aObjs) std::atomic<uInt>();
    new (&self->oObjs) std::atomic<uInt>();

    self->heapSize = 0;
    self->managedBytes=0;
    self->memoryLimit = 0;
    self->adultObjects=0;
    self->youngObjects=0;
    self->oldObjects=0;
    self->yObjs=0;
    self->sleep=false;

#ifdef SHARP_PROF_
    self->x = 0;
    self->largestCollectionTime=0;
    self->collections=0;
    self->timeSpentCollecting=0;
    self->timeSlept=0;
#endif
    self->aObjs=0;
    self->oObjs=0;
    self->isShutdown=false;
    self->messageQueue.init();
    self->locks.init();
}

void GarbageCollector::releaseObject(Object *object) {
    if(object != nullptr && object->object != nullptr)
    {
        object->object->refCount--;
        switch(GENERATION(object->object->info)) {
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
        object->object = nullptr;
    }
}

void GarbageCollector::shutdown() {
    if(self != nullptr) {
        self->isShutdown=true;
#ifdef SHARP_PROF_
        cout << "\nsize of object: " << sizeof(SharpObject) << endl;
        cout << "highest memory calculated: " << hbytes << endl;
        cout << "Objects Collected " << self->x << endl;
        cout << "Total managed bytes left " << self->managedBytes << endl;
        cout << "Objects left over young: " << self->youngObjects << " adult: " << self->adultObjects
                                          << " old: " << self->oldObjects << endl;
        cout << "largest collection " << self->largestCollectionTime << endl;
        cout << "total collections " << self->collections << endl;
        cout << "total time spent collecting " << self->timeSpentCollecting << endl;
        cout << "total time spent sleeping " << self->timeSlept << endl;
        cout << "heap size: " << self->heapSize << endl;
        cout << std::flush << endl;
#endif
        self->locks.free();
        // im no longer freeing memory due to multiple memory references on objects when clearing
        std::free(self); self = nullptr;
    }
}

void GarbageCollector::startup() {
    auto* gcThread=(Thread*)malloc(
            sizeof(Thread)*1);
    gcThread->CreateDaemon("gc");
    Thread::startDaemon(
            GarbageCollector::threadStart, gcThread);
}

void GarbageCollector::collect(CollectionPolicy policy) {
    if(isShutdown)
        return;

    if(policy == GC_LOW) {
        Thread::suspendAllThreads();

        /**
         * To attempt to approve a large memory request we want to take the
         * worst case scenareo route to try to fuffil the memory request. To
         * avoid lags in the application big memory requests should not be
         * performed often
         */
        collectGarbage();

        Thread::resumeAllThreads();
    } else if(policy == GC_EXPLICIT) {
        /**
         * Force collection of both generations
         * We only do the first 2 generations because we want
         * to prevent lag when freeing up memory. the Old generation will
         * always be the largest generation
         */
        if(GC_COLLECT_YOUNG() || GC_COLLECT_ADULT() || GC_COLLECT_OLD()) {
            collectGarbage();
        }
    } else if(policy == GC_CONCURRENT) {

        /**
         * This should only be called by the GC thread itsself
         */
        collectGarbage();
    }

    updateMemoryThreshold();
}

/**
 * We want to update the allocation threshold as the program runs for
 * more efficent memory collection
 *
 */
void GarbageCollector::updateMemoryThreshold() {
    if(GC_LOW_MEM()) {
        memoryThreshold = (0.85 * memoryLimit);
    } else {
        if (samplesReceived == MEMORY_POOL_SAMPLE_SIZE) {
            size_t total = 0, avg;
            for (long i = 0; i < MEMORY_POOL_SAMPLE_SIZE; i++) {
                total += memoryPoolResults[i];
            }

            avg = total / MEMORY_POOL_SAMPLE_SIZE;
            samplesReceived = 0;
            if (avg > memoryThreshold) {
                memoryThreshold = avg; // dynamically update threshold
            } else {
                if (downgradeAttempts++ == MAX_DOWNGRADE_ATTEMPTS) {
                    memoryThreshold = avg; // downgrade memory due to some free operation
                    downgradeAttempts = 0;
                }
            }
        } else {
            memoryPoolResults[samplesReceived++] = managedBytes;
        }
    }
}

void GarbageCollector::collectGarbage() {
    mutex.lock();
    yObjs = 0; aObjs = 0; oObjs = 0;
    SharpObject *object = self->_Mheap->next, *prevObj = self->_Mheap, *cachedTail = NULL;
    cachedTail = tail;
#ifdef SHARP_PROF_
    uInt past = Clock::realTimeInNSecs();
#endif
    mutex.unlock();

    while(object != NULL) {
        if(tself->state == THREAD_KILLED
            || object == cachedTail) {
            break;
        }

        if(GENERATION(object->info) <= gc_old) {
            // free object
            if(MARKED(object->info) && object->refCount == 0) {
                object = sweep(object, prevObj, cachedTail);
                continue;
            } else if(MARKED(object->info) && object->refCount > 0){
                switch(GENERATION(object->info)) {
                    case gc_young:
                        youngObjects--;
                        adultObjects++;
                        SET_GENERATION(object->info, gc_adult);
                        break;
                    case gc_adult:
                        adultObjects--;
                        oldObjects++;
                        SET_GENERATION(object->info, gc_old);
                        break;
                    case gc_old:
                        break;
                }
            } else {
                MARK(object->info, 1);
            }
        }

        prevObj = object;
        object = object->next;
    }

#ifdef SHARP_PROF_
    uInt now = Clock::realTimeInNSecs();
    if(NANO_TOMILL(now-past) > largestCollectionTime)
        largestCollectionTime = NANO_TOMILL(now-past);
    timeSpentCollecting += NANO_TOMILL(now-past);
    collections++;
#endif
}

void GarbageCollector::run() {
#ifdef SHARP_PROF_
    tself->tprof = new Profiler();
    tself->tprof->init(2);
#endif

    for(;;) {
        if(hasSignal(tself->signal, tsig_suspend))
            Thread::suspendSelf();
        if(tself->state == THREAD_KILLED) {
            return;
        }

        message:
        if(!messageQueue.empty()) {
            CollectionPolicy policy;
            {
                GUARD(mutex);
                policy = messageQueue.last();
                messageQueue.pop_back();
            }

            /**
             * We only want to run a concurrent collection
             * in the GC thread itsself
             */
            if(policy != GC_CONCURRENT)
                collect(policy);
        }

        do {

#ifdef SHARP_PROF_
            if(managedBytes > hbytes)
                hbytes = managedBytes;
            timeSlept += 15;
#endif

            __os_yield();
#ifdef WIN32_
            Sleep(15);
#endif
#ifdef POSIX_
            usleep(15*999);
#endif
            if(sleep) sedateSelf();
            if(!messageQueue.empty()) goto message;
        } while(!(GC_COLLECT_MEM() && (GC_COLLECT_YOUNG() || GC_COLLECT_ADULT() || GC_COLLECT_OLD()))
                && !tself->signal);

        if(tself->state == THREAD_KILLED)
            return;

        /**
         * Attempt to collect objects based on the appropriate
         * conditions. This call does not guaruntee that any collections
         * will happen
         */
         collect(GC_CONCURRENT);

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
    self->tself = thread_self;
    Thread::setPriority(thread_self, THREAD_PRIORITY_LOW);

    try {
        self->run();
    } catch(Exception &e){
        /* Should never happen */
        sendSignal(thread_self->signal, tsig_except, 1);
    }

        /*
         * Check for uncaught exception in thread before exit
         */
    thread_self->exit();
#ifdef WIN32_
    return 0;
#endif
#ifdef POSIX_
    return nullptr;
#endif
}

void GarbageCollector::sendMessage(CollectionPolicy message) {
    GUARD(mutex);
    messageQueue.push_back(message);
}

double GarbageCollector::_sizeof(SharpObject *object) {
    double size =0;
    if(object != nullptr) {

        if(TYPE(object->info) == _stype_var) {
            size += sizeof(double)*object->size;
        } else if(TYPE(object->info) == _stype_struct) {
            for(uInt i = 0; i < object->size; i++) {
                SharpObject *o = object->node[i].object;

                /**
                 * If the object still has references we just drop it and move on
                 */
                if(o != nullptr) {
                    size += _sizeof(o);
                }
            }

            size += sizeof(Object)*object->size;
        }

        size += sizeof(SharpObject);
    }

    return size;
}

SharpObject* GarbageCollector::sweep(SharpObject *object, SharpObject *prevObj, SharpObject *tail) {
    if(object != nullptr) {

        sharp_type st = (sharp_type)TYPE(object->info);
        ClassObject *klass = &vm.classes[CLASS(object->info) % vm.manifest.classes];
        int gen = GENERATION(object->info);

        if(TYPE(object->info) == _stype_var) {
            managedBytes -= sizeof(double)*object->size;
            std::free(object->HEAD); object->HEAD = NULL;
        } else if(TYPE(object->info) == _stype_struct) {
            for(unsigned long i = 0; i < object->size; i++) {
                SharpObject *o = object->node[i].object;

                /**
                 * If the object still has references we just drop it and move on
                 */
                DEC_REF(o);
            }

            managedBytes -= sizeof(Object)*object->size;
            std::free(object->node); object->node = NULL;
        }

//        if(object->mutex != NULL)
//            delete (object->mutex);

        UPDATE_GC(object)

#ifdef SHARP_PROF_
        x++;
#endif

        managedBytes -= sizeof(SharpObject);

        SharpObject* nextObj = object->next;
        erase(object, prevObj, tail);
        std::free(object);
        return nextObj;
    }
    return NULL;
}

SharpObject *GarbageCollector::newObject(int64_t size) {
    if(size<=0)
        return nullptr;
    
    SharpObject *object = (SharpObject*)__malloc(sizeof(SharpObject));
    object->init(size, _stype_var);

    object->HEAD = (double*)__calloc(size, sizeof(double));
    SET_TYPE(object->info, _stype_var);

    /* track the allocation amount */
    GUARD(mutex);
    managedBytes += (sizeof(SharpObject)*1)+(sizeof(double)*size);
    PUSH(object);
    youngObjects++;
    heapSize++;

    return object;
}

SharpObject *GarbageCollector::newObjectUnsafe(int64_t size) {
    if(size<=0)
        return nullptr;

    SharpObject *object = (SharpObject*)malloc(sizeof(SharpObject));
    if(object != NULL) {
        object->init(size, _stype_var);

        object->HEAD = (double *) calloc(size, sizeof(double));
        if(object->HEAD != NULL) {
            SET_TYPE(object->info, _stype_var);

            /* track the allocation amount */
            GUARD(mutex);
            managedBytes += (sizeof(SharpObject) * 1) + (sizeof(double) * size);
            PUSH(object);
            youngObjects++;
            heapSize++;
        } else {
            std::free(object);
            return NULL;
        }
    }

    return object;
}

SharpObject *GarbageCollector::newObject(ClassObject *k, bool staticInit) {
    if(k != nullptr) {
        SharpObject *object = (SharpObject*)__malloc(sizeof(SharpObject));
        uint32_t size = staticInit ? k->staticFields : k->instanceFields;

        object->init(size, k);
        if(size > 0) {
            object->node = (Object*)__calloc(size, sizeof(Object));
            uInt fieldAddress =  staticInit ? k->instanceFields : 0;

            for(unsigned int i = 0; i < object->size; i++) {
                /**
                 * We want to set the class variables and arrays
                 * to null and initialize the var variables
                 */
                if(k->fields[fieldAddress].type <= VAR && !k->fields[fieldAddress].isArray) {
                    if(!staticInit || (staticInit && IS_STATIC(k->fields[fieldAddress].flags))) {
                        object->node[i] = newObject(1);
                    }
                }

                fieldAddress++;
            }

        }

        GUARD(mutex);
        managedBytes += (sizeof(SharpObject)*1)+(sizeof(Object)*size);
        PUSH(object);
        youngObjects++;
        heapSize++;
        return object;
    }

    return nullptr;
}

SharpObject *GarbageCollector::newObjectArray(int64_t size) {
    if(size<=0)
        return nullptr;
    
    SharpObject *object = (SharpObject*)__malloc(sizeof(SharpObject));
    object->init(size, _stype_struct);
    object->node = (Object*)__calloc(size, sizeof(Object)*1);
    
    /* track the allocation amount */
    GUARD(mutex);
    managedBytes += (sizeof(SharpObject)*1)+(sizeof(Object)*size);
    PUSH(object);
    youngObjects++;
    heapSize++;

    return object;
}

SharpObject *GarbageCollector::newObjectArray(int64_t size, ClassObject *k) {
    if(k != nullptr && size > 0) {
        
        SharpObject *object = (SharpObject*)__malloc(sizeof(SharpObject)*1);
        object->init(size, k);

        if(size > 0)
            object->node = (Object*)__calloc(size, sizeof(Object));

        /* track the allocation amount */
        GUARD(mutex);
        managedBytes += (sizeof(SharpObject)*1)+(sizeof(Object)*size);
        PUSH(object);
        youngObjects++;
        heapSize++;

        return object;
    }

    return nullptr;
}

void GarbageCollector::createStringArray(Object *object, runtime::String& str) {
    if(object != nullptr) {
        *object = newObject(str.len);

        if(object->object != NULL) {
            double *array = object->object->HEAD;
            for (unsigned long i = 0; i < str.len; i++) {
                *array = str.chars[i];
                array++;
            }
        }
    }
}

uInt GarbageCollector::getMemoryLimit() {
    return memoryLimit;
}

uInt GarbageCollector::getManagedMemory() {
    return managedBytes;
}

SharpObject* GarbageCollector::erase(SharpObject *freedObj, SharpObject *prevObj, SharpObject *tail) {
    heapSize--;

    if(HAS_LOCK(freedObj->info))
        dropLock(freedObj);

    if(tail == freedObj){
        GUARD(mutex);
        prevObj->next = freedObj->next;
    } else prevObj->next = freedObj->next;
    return prevObj->next;
}

void GarbageCollector::realloc(SharpObject *o, size_t sz) {
    if(o != NULL && TYPE(o->info) == _stype_var) {
        o->HEAD = (double*)__realloc(o->HEAD, sizeof(double)*sz, sizeof(double)*o->size);

        GUARD(mutex);
        if(sz < o->size)
            managedBytes -= (sizeof(double)*(o->size-sz));
        else
            managedBytes += (sizeof(double)*(sz-o->size));

        if(sz > o->size)
            std::memset(o->HEAD+o->size, 0, sizeof(double)*(sz-o->size));
        o->size = sz;
    }
}

void GarbageCollector::reallocObject(SharpObject *o, size_t sz) {
    if(o != NULL && TYPE(o->info) == _stype_struct && o->node != NULL) {
        if(sz < o->size) {
            for(size_t i = sz; i < o->size; i++) {
                if(o->node[i].object != nullptr) {
                    DEC_REF(o->node[i].object)
                }
            }
        }

        o->node = (Object*)__realloc(o->node, sizeof(Object)*sz, sizeof(Object)*o->size);
        GUARD(mutex);
        if(sz < o->size)
            managedBytes -= (sizeof(Object)*(o->size-sz));
        else
            managedBytes += (sizeof(Object)*(sz-o->size));


        if(sz > o->size)
            std::memset(o->node+o->size, 0, sizeof(Object)*(sz-o->size));
        o->size = sz;
    }
}

void GarbageCollector::kill() {
    mutex.lock();
    if(tself->state == THREAD_RUNNING) {
        tself->state = THREAD_KILLED;
        sendSignal(tself->signal, tsig_kill, 1);
        Thread::waitForThreadExit(tself);
    }

    mutex.unlock();
}

void GarbageCollector::sedateSelf() {
    Thread* self = thread_self;
    self->suspended = true;
    self->state = THREAD_SUSPENDED;
    while(sleep) {

#ifdef SHARP_PROF_
        if(managedBytes > hbytes)
                hbytes = managedBytes;
#endif

#ifdef WIN32_
        Sleep(30);
#endif
#ifdef POSIX_
        usleep(30*999);
#endif
        if(self->state != THREAD_RUNNING)
            break;
    }

    // we don't want to shoot ourselves in the foot
    if(self->state == THREAD_SUSPENDED)
        self->state = THREAD_RUNNING;
    self->suspended = false;
}

bool isLocker(void *o, mutex_t* mut) {
    return (SharpObject*)o == mut->object;
}

void GarbageCollector::lock(SharpObject *o, Thread* thread) {
    if(o) {
        mutex_t *mut;
        mutex.lock();
        long long idx = locks.indexof(isLocker, o);
        if(idx != -1)
            mut = locks.get(idx);
        else {
            managedBytes += sizeof(mutex_t)+sizeof(recursive_mutex);
            mut = new mutex_t(o, new recursive_mutex(), -1);
            locks.add(mut);
            SET_LOCK(o->info, 1);
        }
        mutex.unlock();
        if(mut->threadid != thread->id) {
            mut->mutex->lock();
            mut->threadid = thread->id;
        }
    }
}

void GarbageCollector::unlock(SharpObject *o, Thread* thread) {
    if(o) {
        mutex_t *mut=0;
        mutex.lock();
        long long idx = locks.indexof(isLocker, o);
        if(idx != -1)
            mut = locks.get(idx);
        mutex.unlock();
        if(mut && mut->threadid==thread->id) {
            mut->threadid = -1;
            mut->mutex->unlock();
        } 
    }
}

void GarbageCollector::reconcileLocks(Thread* thread) {

    mutex.lock();
    for(long long i = 0; i < locks.size(); i++) {
        mutex_t *mut = locks.get(i);
        if(mut->threadid==thread->id) {
            mut->threadid = -1;
            mut->mutex->unlock();
        }
    }
    mutex.unlock();
}

void GarbageCollector::dropLock(SharpObject *o) {
    if(o) {
        mutex.lock();
        long long idx = locks.indexof(isLocker, o);
        if(idx != -1) {
            mutex_t *mut = locks.get(idx);
            if(mut->threadid!= -1)
                mut->mutex->unlock();
            managedBytes -= sizeof(mutex_t) + sizeof(recursive_mutex);
            delete mut->mutex;
            delete mut;
            locks.remove(idx);
        }
        mutex.unlock();
    }
}

void GarbageCollector::sedate() {
    mutex.lock();
    if(!sleep && tself->state == THREAD_RUNNING) {
        sleep = true;
        Thread::suspendAndWait(Thread::getThread(gc_threadid));
    }
    mutex.unlock();
}

void GarbageCollector::wake() {
    mutex.lock();
    if(sleep) {
        sleep = false;
        Thread::waitForThread(Thread::getThread(gc_threadid));
    }
    mutex.unlock();
}

int GarbageCollector::selfCollect() {
    if(sleep || tself->state == THREAD_KILLED) {
        mutex.lock();
        collectGarbage();
        mutex.unlock();
        return 0;
    }

    return -1;
}

bool GarbageCollector::isAwake() {
    return !sleep && tself->state == THREAD_RUNNING;
}
