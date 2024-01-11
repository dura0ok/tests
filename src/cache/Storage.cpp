#include "Storage.h"

CacheElement *Storage::getElement(const std::string &key) {
    pthread_rwlock_rdlock(&dataMapLock);
    auto it = dataMap.find(key);
    CacheElement *result = (it != dataMap.end()) ? it->second.get() : nullptr;
    pthread_rwlock_unlock(&dataMapLock);
    return result;
}

void Storage::initElement(const std::string &key) {
    pthread_rwlock_wrlock(&dataMapLock);
    dataMap.emplace(key, std::make_unique<CacheElement>());
    pthread_rwlock_unlock(&dataMapLock);
}


bool Storage::containsKey(const std::string &key) const {
    pthread_rwlock_rdlock(&dataMapLock);
    bool result = (dataMap.find(key) != dataMap.end());
    pthread_rwlock_unlock(&dataMapLock);
    return result;
}

void Storage::clearElement(const std::string &key) {
    pthread_rwlock_wrlock(&dataMapLock);
    dataMap.erase(key);
    pthread_rwlock_unlock(&dataMapLock);
}