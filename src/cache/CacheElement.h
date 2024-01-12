#pragma once

#include <string>
#include <map>
#include <mutex>
#include <vector>
#include <sys/poll.h>
#include <shared_mutex>
#include "../ClientInfo.h"
#include <atomic>

class CacheElement {
public:
    CacheElement() {
        pthread_rwlock_init(&mData, nullptr);
        pthread_rwlock_init(&mUserBufStates, nullptr);
    }

    ~CacheElement() {
        pthread_rwlock_destroy(&mData);
        pthread_rwlock_destroy(&mUserBufStates);
    }


    bool isFinishReading(ssize_t offset);

    void markFinished();

    void initReader(ClientInfo *info);

    size_t readData(char *buf, size_t buf_size, ssize_t offset);

    void appendData(const char *buf, size_t size);

    void makeReadersReadyToWrite();

    size_t getDataSize() {
        return data.size();
    }

    void decrementReadersCount() {
        readersCount.fetch_sub(1, std::memory_order_relaxed);
    }

    bool isReadersEmpty() {
        size_t expected = 0;
        size_t tmp = 0;
        if (readersCount.compare_exchange_strong(expected, tmp)) {
            return true;
        }
        return false;
    }

    int getStatusCode();

private:

    std::map<int, ClientInfo *> userBufStates;
    pthread_rwlock_t mData{};
    pthread_rwlock_t mUserBufStates{};
    std::string data;
    std::atomic_size_t readersCount;
    bool finished{};
};
