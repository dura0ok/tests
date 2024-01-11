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
    worker->storeClientConnection(fd);
    current_thread++;
    current_thread = current_thread % pool_size;
}