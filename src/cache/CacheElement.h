#pragma once

#include <string>
#include <map>
#include <mutex>
#include <vector>
#include <sys/poll.h>
#include <shared_mutex>
#include "../ClientInfo.h"

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

    void initReader(ClientInfo* info);

    std::string readData(ssize_t offset);

    void appendData(const std::string &new_data);

    void makeReadersReadyToWrite();

private:

    std::map<int, ClientInfo*> userBufStates;
    pthread_rwlock_t mData{};
    pthread_rwlock_t mUserBufStates{};
    std::string data;
    bool finished{};
};
