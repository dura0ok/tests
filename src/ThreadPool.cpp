#include <iostream>
#include "ThreadPool.h"

ThreadPool::ThreadPool(size_t size) : pool_size(size) {
    workers.reserve(pool_size);
    for (size_t i = 0; i < pool_size; ++i) {
        workers.emplace_back(std::make_unique<ThreadWorker>(cacheStorage));
    }
}

void ThreadPool::AddFDToWorker(int fd) {
    std::cout << "Add fd " << fd << std::endl;
    auto worker = workers[current_thread].get();
    worker->addPipe(fd);
    incrementCurrentThread();
}

void ThreadPool::incrementCurrentThread() {
    size_t localCurrentThread;
    size_t newCurrentThread;
    do {
        localCurrentThread =   current_thread.load(std::memory_order_relaxed);
        newCurrentThread = ( localCurrentThread + 1 ) % pool_size;
    }while (current_thread.compare_exchange_strong(localCurrentThread, newCurrentThread));
}
