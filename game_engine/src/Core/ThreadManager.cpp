#include "Core/ThreadManager.h"
#include <iostream>
#include <cstring>
#include <cstdint>

#ifdef LINUX_BUILD
    #include <thread>
    #include <chrono>
#else
    #include <vitasdk.h>
    #include <psp2/kernel/threadmgr.h>
#endif

namespace GameEngine {

ThreadManager& ThreadManager::getInstance() {
    static ThreadManager instance;
    return instance;
}

#ifdef LINUX_BUILD
ThreadHandle ThreadManager::createThread(const std::string& name, std::function<void()> func) {
    auto thread = std::make_unique<std::thread>([name, func]() {
        #ifdef __linux__
            pthread_setname_np(pthread_self(), name.c_str());
        #endif
        func();
    });
    
    ThreadHandle handle = std::move(*thread);
    threads.push_back(std::move(thread));
    
    std::cout << "ThreadManager: Created thread '" << name << "' (Total threads: " << threads.size() << ")" << std::endl;
    
    return handle;
}

void ThreadManager::joinThread(ThreadHandle& handle) {
    if (handle.joinable()) {
        handle.join();
    }
}

bool ThreadManager::isValid(const ThreadHandle& handle) const {
    return handle.joinable();
}

void ThreadManager::sleep(int milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

uint64_t ThreadManager::getCurrentThreadId() const {
    return std::hash<std::thread::id>{}(std::this_thread::get_id());
}

#else
typedef void (*ThreadFunc)(void* context);

struct ThreadData {
    ThreadFunc func;
    void* context;
    std::string name;
    
    ThreadData(ThreadFunc f, void* ctx, const std::string& n) 
        : func(f), context(ctx), name(n) {
    }
};

static int vitaThreadEntry(SceSize args, void* argp) {
    std::cout << "vitaThreadEntry: Thread entry called (args=" << args << ")" << std::endl;
    
    if (!argp || args < sizeof(ThreadData*)) {
        std::cerr << "vitaThreadEntry: Invalid arguments!" << std::endl;
        sceKernelExitDeleteThread(-1);
        return -1;
    }
    
    ThreadData* data = nullptr;
    memcpy(&data, argp, sizeof(ThreadData*));
    
    std::cout << "vitaThreadEntry: Read ThreadData* = " << data << std::endl;
    
    uintptr_t dataAddr = reinterpret_cast<uintptr_t>(data);
    if (!data || dataAddr < 0x1000 || dataAddr > 0x90000000) {
        std::cerr << "vitaThreadEntry: Invalid ThreadData pointer! (addr=" << std::hex << dataAddr << std::dec << ")" << std::endl;
        sceKernelExitDeleteThread(-1);
        return -1;
    }
    
    if (!data->func) {
        std::cerr << "vitaThreadEntry: No function pointer!" << std::endl;
        delete data;
        sceKernelExitDeleteThread(-1);
        return -1;
    }
    
    std::cout << "vitaThreadEntry: About to call function, context=" << data->context << std::endl;
    
    data->func(data->context);
    
    std::cout << "vitaThreadEntry: Function call completed" << std::endl;
    
    delete data;
    
    sceKernelExitDeleteThread(0);
    return 0;
}

static void callStdFunction(void* context) {
    if (!context) {
        return;
    }
    
    struct FuncContext {
        std::function<void()> func;
        FuncContext(const std::function<void()>& f) : func(f) {}
    };
    
    FuncContext* ctx = static_cast<FuncContext*>(context);
    if (ctx) {
        if (ctx->func) {
            ctx->func();
        }
        delete ctx;
    }
}

ThreadHandle ThreadManager::createThread(const std::string& name, std::function<void()> func) {
    ThreadHandle handle;
    handle.valid = false;
    handle.threadId = 0;
    
    struct FuncContext {
        std::function<void()> func;
        FuncContext(const std::function<void()>& f) : func(f) {}
    };
    
    FuncContext* ctx = new (std::nothrow) FuncContext(func);
    if (!ctx) {
        std::cerr << "ThreadManager: Failed to allocate context for thread: " << name << std::endl;
        return handle;
    }
    
    if (!callStdFunction) {
        delete ctx;
        std::cerr << "ThreadManager: Function pointer is invalid!" << std::endl;
        return handle;
    }
    
    ThreadData* data = new (std::nothrow) ThreadData(callStdFunction, ctx, name);
    if (!data) {
        delete ctx;
        std::cerr << "ThreadManager: Failed to allocate memory for thread data: " << name << std::endl;
        return handle;
    }
    
    if (!data->func || data->func != callStdFunction) {
        delete data;
        std::cerr << "ThreadManager: Function pointer not set correctly!" << std::endl;
        return handle;
    }
    
    SceUID threadId = sceKernelCreateThread(
        name.c_str(),
        vitaThreadEntry,
        0x10000100,
        512 * 1024,
        0,
        0,
        nullptr
    );
    
    if (threadId >= 0) {
        ThreadData* dataPtr = data;
        int result = sceKernelStartThread(threadId, sizeof(ThreadData*), &dataPtr);
    
        if (result >= 0) {
            handle.threadId = threadId;
        handle.valid = true;
            threads.push_back(threadId);
            std::cout << "ThreadManager: Created thread '" << name << "' (ID: " << threadId << ", Total threads: " << threads.size() << ")" << std::endl;
        } else {
            sceKernelDeleteThread(threadId);
            delete data;
            std::cerr << "ThreadManager: Failed to start thread: " << name << " (error: " << result << ")" << std::endl;
        }
    } else {
        delete data;
        std::cerr << "ThreadManager: Failed to create thread: " << name << " (error: " << threadId << ")" << std::endl;
    }
    
    return handle;
}

void ThreadManager::joinThread(ThreadHandle& handle) {
    if (handle.valid && handle.threadId >= 0) {
        int stat = 0;
        sceKernelWaitThreadEnd(handle.threadId, &stat, nullptr);
        handle.valid = false;
        handle.threadId = 0;
    }
}

bool ThreadManager::isValid(const ThreadHandle& handle) const {
    return handle.valid && handle.threadId >= 0;
}

void ThreadManager::sleep(int milliseconds) {
    sceKernelDelayThread(milliseconds * 1000);
}

uint64_t ThreadManager::getCurrentThreadId() const {
    return static_cast<uint64_t>(sceKernelGetThreadId());
}
#endif

size_t ThreadManager::getThreadCount() const {
    return threads.size();
}

} // namespace GameEngine

