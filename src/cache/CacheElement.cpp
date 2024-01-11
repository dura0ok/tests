#include "CacheElement.h"
#include "../Config.h"

bool CacheElement::isFinishReading(int sock_fd) {
    pthread_rwlock_rdlock(&readerlock);
    size_t offset = userBufStates.at(sock_fd);
    pthread_rwlock_unlock(&readerlock);
    return finished && (offset == data.size());
}

void CacheElement::markFinished() {
    finished = true;
}

void CacheElement::clearReader(int sock_fd) {
    pthread_rwlock_wrlock(&readerlock);
    userBufStates.erase(sock_fd);
    pthread_rwlock_unlock(&readerlock);
}

void CacheElement::initReader(int sock_fd) {
    pthread_rwlock_wrlock(&readerlock);
    userBufStates.insert(std::make_pair(sock_fd, 0));
    pthread_rwlock_unlock(&readerlock);
}

std::string CacheElement::readData(int sock_fd) {
    pthread_rwlock_rdlock(&readerlock);
    auto &offset = userBufStates.at(sock_fd);
    pthread_rwlock_unlock(&readerlock);
    pthread_rwlock_rdlock(&rwlock);
    auto res = data.substr(offset, CHUNK_SIZE);
    offset += static_cast<long>(res.size());
    pthread_rwlock_unlock(&rwlock);
    return res;
}

void CacheElement::appendData(const std::string &new_data) {
    pthread_rwlock_wrlock(&rwlock);
    data += new_data;
    pthread_rwlock_unlock(&rwlock);
}

size_t CacheElement::getReadersCount() {
    pthread_rwlock_rdlock(&readerlock);
    size_t result = userBufStates.size();
    pthread_rwlock_unlock(&readerlock);
    return result;
}

bool CacheElement::isFinished() const {
    return finished;
}

void CacheElement::makeReadersReadyToWrite(std::vector<pollfd> &fds) {
    for (auto & userBufState : userBufStates) {
        auto userOffset = static_cast<size_t>(userBufState.second);
        if (userOffset < data.size()){
            fds[userBufState.first].events |= POLLOUT;
        }
    }
}

