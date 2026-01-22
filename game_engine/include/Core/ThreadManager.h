#ifndef THREAD_MANAGER_H
#define THREAD_MANAGER_H

#include <functional>
#include <memory>
#include <atomic>
#include <queue>
#include <iostream>

#ifdef LINUX_BUILD
    #include <thread>
    #include <mutex>
    #include <condition_variable>
#else
    #include <psp2/types.h>
    #include <psp2/kernel/threadmgr.h>
#endif

namespace GameEngine {

#ifdef LINUX_BUILD
    using ThreadHandle = std::thread;
#else
    struct ThreadHandle {
        SceUID threadId;
        bool valid;
        ThreadHandle() : threadId(0), valid(false) {}
    };
#endif

#ifdef LINUX_BUILD
    using Mutex = std::mutex;
    using LockGuard = std::lock_guard<std::mutex>;
    using UniqueLock = std::unique_lock<std::mutex>;
    using ConditionVariable = std::condition_variable;
#else
    struct Mutex {
        SceUID mutexId;
        const char* mutexName;
        
        Mutex(const char* name = "EngineMutex") : mutexName(name) { 
            mutexId = sceKernelCreateMutex(name, 0x02, 1, nullptr);
            if (mutexId < 0) {
                std::cerr << "Mutex '" << name << "' creation failed: " << mutexId << std::endl;
            }
        }
        ~Mutex() { 
            if (mutexId >= 0) {
                sceKernelDeleteMutex(mutexId);
            }
        }
        void lock() { 
            if (mutexId >= 0) {
                int result = sceKernelLockMutex(mutexId, 1, nullptr);
                if (result < 0) {
                    if (result == 0x80028142) {
                        std::cerr << "Mutex '" << mutexName << "' (ID: " << mutexId << ") recursive lock error!" << std::endl;
                        std::cerr.flush();
                    } else {
                        std::cerr << "Mutex '" << mutexName << "' (ID: " << mutexId << ") lock failed: 0x" << std::hex << result << std::dec << std::endl;
                        std::cerr.flush();
                    }
                }
            }
        }
        void unlock() { 
            if (mutexId >= 0) {
                int result = sceKernelUnlockMutex(mutexId, 1);
                if (result < 0) {
                    std::cerr << "Mutex::unlock() failed: 0x" << std::hex << result << std::dec << std::endl;
                }
            }
        }
        bool try_lock() { 
            if (mutexId >= 0) {
                return sceKernelTryLockMutex(mutexId, 1) == 0;
            }
            return false;
        }
        bool try_lock_timeout(unsigned int timeoutMs) {
            if (mutexId >= 0) {
                unsigned int timeout = timeoutMs * 1000;
                int result = sceKernelLockMutex(mutexId, 1, &timeout);
                return result == 0;
            }
            return false;
        }
        SceUID getMutexId() const { return mutexId; }
    };
    
    struct LockGuard {
        Mutex& m;
        LockGuard(Mutex& mutex) : m(mutex) { m.lock(); }
        ~LockGuard() { m.unlock(); }
    };
    
    struct UniqueLock {
        Mutex& m;
        bool locked;
        UniqueLock(Mutex& mutex) : m(mutex), locked(false) { 
            int result = sceKernelLockMutex(m.getMutexId(), 1, nullptr);
            if (result == 0) {
                locked = true;
            } else {
                std::cerr << "UniqueLock: Failed to lock mutex (ID: " << m.getMutexId() << ", result: " << result << ")" << std::endl;
                std::cerr.flush();
                locked = false;
            }
        }
        void lock() { 
            if (!locked) { 
                int result = sceKernelLockMutex(m.getMutexId(), 1, nullptr);
                if (result == 0) {
                    locked = true;
                }
            } 
        }
        void unlock() { 
            if (locked) { 
                m.unlock(); 
                locked = false;
            }
        }
        ~UniqueLock() { if (locked) unlock(); }
        SceUID getMutexId() const { return m.getMutexId(); }
        bool isLocked() const { return locked; }
    };
    
    struct ConditionVariable {
        SceUID condId;
        SceUID mutexId;
        bool initialized;
        
        ConditionVariable() : condId(-1), mutexId(0), initialized(false) {}
        
        void init(SceUID associatedMutexId) {
            if (!initialized && associatedMutexId >= 0) {
                mutexId = associatedMutexId;
                char condName[32];
                snprintf(condName, sizeof(condName), "EngineCond_%d", associatedMutexId);
                condId = sceKernelCreateCond(condName, 0, mutexId, nullptr);
                initialized = (condId >= 0);
                if (initialized) {
                    std::cout << "ConditionVariable::init: Created cond (ID: " << condId << ", mutex ID: " << mutexId << ", name: " << condName << ")" << std::endl;
                    std::cout.flush();
                } else {
                    std::cerr << "ConditionVariable::init: Failed to create cond (mutex ID: " << mutexId << ")" << std::endl;
                    std::cerr.flush();
                }
            }
        }
        
        ~ConditionVariable() { 
            if (condId >= 0) {
                sceKernelDeleteCond(condId);
            }
        }
        void wait(UniqueLock& lock) { 
            if (initialized && condId >= 0 && lock.getMutexId() == mutexId) {
                std::cout << "ConditionVariable::wait: About to wait on cond (ID: " << condId << ", mutex ID: " << mutexId << ")" << std::endl;
                std::cout.flush();
                int result = sceKernelWaitCond(condId, nullptr);
                std::cout << "ConditionVariable::wait: Wait returned (result: " << result << ")" << std::endl;
                std::cout.flush();
            } else {
                std::cerr << "ConditionVariable::wait: Not initialized or invalid! (initialized: " << initialized 
                          << ", condId: " << condId << ", mutexId: " << mutexId << ", lock mutexId: " << lock.getMutexId() << ")" << std::endl;
                std::cerr.flush();
            }
        }
        void notify_one() { 
            if (initialized && condId >= 0) {
                sceKernelSignalCond(condId);
            }
        }
        void notify_all() { 
            if (initialized && condId >= 0) {
                sceKernelSignalCondAll(condId);
            }
        }
    };
#endif

template<typename T>
class ThreadSafeQueue {
private:
    const char* queueName;
    int pushCount;
    int popCount;
    
public:
    ThreadSafeQueue(const char* name = "ThreadSafeQueue") 
        #ifndef LINUX_BUILD
        : mutex(name), queueName(name), pushCount(0), popCount(0)
        #else
        : queueName(name), pushCount(0), popCount(0)
        #endif
    {
        #ifndef LINUX_BUILD
        condition.init(mutex.getMutexId());
        #endif
    }
    
    void push(const T& item) {
        LockGuard lock(mutex);
        queue.push(item);
        condition.notify_one();
    }
    
    bool tryPop(T& item) {
        #ifndef LINUX_BUILD
        if (mutex.mutexId < 0) {
            std::cerr << "ThreadSafeQueue '" << queueName << "'::tryPop: Invalid mutex!" << std::endl;
            return false;
        }
        bool locked = mutex.try_lock_timeout(50);
        #else
        bool locked = mutex.try_lock();
        #endif
        if (!locked) {
            return false;
        }
        if (queue.empty()) {
            mutex.unlock();
            return false;
        }
        item = queue.front();
        queue.pop();
        mutex.unlock();
        return true;
    }
    
    bool pop(T& item) {
        UniqueLock lock(mutex);
        #ifndef LINUX_BUILD
        if (!lock.isLocked()) {
            return false;
        }
        #endif
        
        while (queue.empty() && !shouldStop) {
            condition.wait(lock);
        }
        
        if (shouldStop && queue.empty()) {
            return false;
        }
        
        item = queue.front();
        queue.pop();
        return true;
    }
    
    bool empty() const {
        LockGuard lock(mutex);
        return queue.empty();
    }
    
    size_t size() const {
        LockGuard lock(mutex);
        return queue.size();
    }
    
    void stop() {
        LockGuard lock(mutex);
        shouldStop = true;
        condition.notify_all();
    }
    
    void reset() {
        LockGuard lock(mutex);
        shouldStop = false;
        while (!queue.empty()) {
            queue.pop();
        }
    }
    
private:
    mutable Mutex mutex;
    ConditionVariable condition;
    std::queue<T> queue;
    std::atomic<bool> shouldStop{false};
};

class ThreadManager {
public:
    static ThreadManager& getInstance();
    
    ThreadHandle createThread(const std::string& name, std::function<void()> func);
    
    void joinThread(ThreadHandle& handle);
    
    bool isValid(const ThreadHandle& handle) const;
    
    void sleep(int milliseconds);
    
    uint64_t getCurrentThreadId() const;
    
    size_t getThreadCount() const;
    
private:
    ThreadManager() = default;
    ~ThreadManager() = default;
    ThreadManager(const ThreadManager&) = delete;
    ThreadManager& operator=(const ThreadManager&) = delete;
    
#ifdef LINUX_BUILD
    std::vector<std::unique_ptr<std::thread>> threads;
#else
    std::vector<SceUID> threads;
#endif
};

} // namespace GameEngine

#endif // THREAD_MANAGER_H


