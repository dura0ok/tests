#pragma once

#include <vector>
#include <sys/poll.h>
#include <cstddef>
#include <mutex>
#include <memory>
#include "ThreadWorker.h"
#include "cache/Storage.h"

class ThreadPool {
public:
    explicit ThreadPool(size_t pool_size);

    void AddFDToWorker(int fd);

private:
    std::vector<std::unique_ptr<ThreadWorker>> workers;
    size_t current_thread = 0;
    size_t pool_size = 0;
    Storage cacheStorage;
};