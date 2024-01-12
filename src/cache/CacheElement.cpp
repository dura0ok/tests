#include "CacheElement.h"
#include "../Config.h"
#include "../ClientInfo.h"
#include "../ThreadPool.h"

bool CacheElement::isFinishReading(ssize_t offset) {
    pthread_rwlock_rdlock(&readerLock);
    ssize_t size = data.size();
    auto localFinished = finished;
    pthread_rwlock_unlock(&readerLock);
    return localFinished && (offset == size);
}

void CacheElement::markFinished() {
    pthread_rwlock_rdlock(&readerLock);
    finished = true;
    pthread_rwlock_unlock(&readerLock);
}

void CacheElement::clearReader(int sock_fd) {
    pthread_rwlock_wrlock(&rwlock);
    userBufStates.erase(sock_fd);
    pthread_rwlock_unlock(&rwlock);
}

void CacheElement::initReader(int sock_fd, ssize_t offset) {
    pthread_rwlock_wrlock(&rwlock);
    userBufStates.insert(std::make_pair(sock_fd, offset));
    pthread_rwlock_unlock(&rwlock);
}

std::string CacheElement::readData(ssize_t offset) {
    pthread_rwlock_rdlock(&readerLock);
    auto res = data.substr(offset, CHUNK_SIZE);
    pthread_rwlock_unlock(&readerLock);
    return res;
}

void CacheElement::appendData(const std::string &new_data) {
    pthread_rwlock_wrlock(&rwlock);
    data += new_data;
    pthread_rwlock_unlock(&rwlock);
}

size_t CacheElement::getReadersCount() {
    pthread_rwlock_rdlock(&readerLock);
    size_t result = userBufStates.size();
    pthread_rwlock_unlock(&readerLock);
    return result;
}

bool CacheElement::isFinished() const {
    return finished;
}

void CacheElement::makeReadersReadyToWrite(const std::string &uri) {
    ClientInfo info;
    info.uri = uri;
    assert(pool);
    for (auto &userBufState: userBufStates) {
        info.fd = userBufState.first;
        info.offset = userBufState.second;
        pool->AddClientInfoToWorker(info);
//        auto userOffset = static_cast<size_t>(userBufState.second);
//        if (userOffset < data.size()) {
//            fds[userBufState.first].events |= POLLOUT;
//        }
    }
}

