#pragma once

#include <map>
#include <string>
#include <memory>
#include "CacheElement.h"

class Storage {
public:
    Storage() {
        pthread_rwlock_init(&dataMapLock, nullptr);
    }

    ~Storage() {
        pthread_rwlock_destroy(&dataMapLock);
    }

    CacheElement *getElement(const std::string &key);

    [[nodiscard]] bool containsKey(const std::string &key) const;

    void initElement(const std::string &key);

    void clearElement(const std::string &key);

    void lock(){
        pthread_mutex_lock(&mutex);
    }

    void unlock(){
        pthread_mutex_unlock(&mutex);
    }

private:
    std::map<std::string, std::unique_ptr<CacheElement>> dataMap;
    mutable pthread_rwlock_t dataMapLock;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
};