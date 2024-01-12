#pragma once

#include <string>
#include <map>
#include <mutex>
#include <vector>
#include <sys/poll.h>
#include <shared_mutex>

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

    bool isFinished() const;

    void initReader(int sock_fd, ssize_t offset);

    std::string readData(ssize_t offset);

    void appendData(const std::string &new_data);

    void clearReader(int sock_fd);


    size_t getReadersCount();

    void makeReadersReadyToWrite(const std::string &uri);

private:

    std::map<int, ssize_t> userBufStates;
    pthread_rwlock_t mData{};
    pthread_rwlock_t mUserBufStates{};
    std::string data;
    bool finished{};
};
