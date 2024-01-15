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

    bool clearElement(const std::string &key);

private:
    std::map<std::string, std::unique_ptr<CacheElement>> dataMap;
    mutable pthread_rwlock_t dataMapLock;
};