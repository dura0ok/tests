#include "CacheElement.h"
#include "../Config.h"
#include "../ClientInfo.h"
#include "../ThreadPool.h"

bool CacheElement::isFinishReading(ssize_t offset) {
    pthread_rwlock_rdlock(&mData);
    ssize_t size = data.size();
    auto localFinished = finished;
    pthread_rwlock_unlock(&mData);
    return localFinished && (offset == size);
}

void CacheElement::markFinished() {
    pthread_rwlock_wrlock(&mUserBufStates);
    finished = true;
    pthread_rwlock_unlock(&mUserBufStates);
}

void CacheElement::initReader(ClientInfo *info) {
    pthread_rwlock_wrlock(&mUserBufStates);
    userBufStates.insert(std::make_pair(info->fd, info));
    pthread_rwlock_unlock(&mUserBufStates);
}

std::string CacheElement::readData(ssize_t offset) {
    pthread_rwlock_rdlock(&mData);
    auto res = data.substr(offset, CHUNK_SIZE);
    pthread_rwlock_unlock(&mData);
    return res;
}

void CacheElement::appendData(const std::string &new_data) {
    pthread_rwlock_wrlock(&mData);
    data += new_data;
    pthread_rwlock_unlock(&mData);
}

void CacheElement::makeReadersReadyToWrite() {
    pthread_rwlock_wrlock(&mUserBufStates);
    for (auto &userBufState: userBufStates) {
        pool->AddClientInfoToWorker(userBufState.second);
    }
    userBufStates.clear();
    pthread_rwlock_unlock(&mUserBufStates);
}

