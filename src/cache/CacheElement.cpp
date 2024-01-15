#include "CacheElement.h"
#include "../ThreadPool.h"
#include <cstring>

bool CacheElement::isFinishReading(ssize_t offset) {
    pthread_rwlock_rdlock(&mData);
    ssize_t size = data.size();
    auto localFinished = isFinished();
    pthread_rwlock_unlock(&mData);
    return localFinished && (offset == size);
}

void CacheElement::markFinished() {
    pthread_rwlock_rdlock(&mUserBufStates);
    const int localStatusCode = HttpParser::parseStatusCode(data);
    pthread_rwlock_unlock(&mUserBufStates);
    pthread_rwlock_wrlock(&mUserBufStates);
    finished = true;
    statusCode = localStatusCode;
    pthread_rwlock_unlock(&mUserBufStates);
}


void CacheElement::initReader(ClientInfo *info) {
    pthread_rwlock_wrlock(&mUserBufStates);
    userBufStates.insert(std::make_pair(info->fd, info));
    pthread_rwlock_unlock(&mUserBufStates);
}

size_t CacheElement::readData(char *buf, size_t buf_size, ssize_t offset) {
    auto copySize = (data.size() >= (buf_size + offset)) ? buf_size : (data.size() - offset);
    pthread_rwlock_rdlock(&mData);
    std::memcpy(buf, data.data() + offset, copySize);
    pthread_rwlock_unlock(&mData);
    return copySize;
}

void CacheElement::appendData(const char *buf, size_t size) {
    pthread_rwlock_wrlock(&mData);
    data.append(std::string_view(buf, size));
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

int CacheElement::getStatusCode() {
    return statusCode;
}

bool CacheElement::isFinished() const {
    return finished;
}

