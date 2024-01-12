#include <sys/poll.h>
#include <bits/poll.h>
#include <sys/socket.h>
#include <iostream>
#include <algorithm>
#include <csignal>
#include <fcntl.h>
#include <cstring>
#include "ThreadWorker.h"
#include "Config.h"
#include "helpers/HttpParser.h"
#include "helpers/HostConnector.h"

ThreadWorker::ThreadWorker(Storage &newStorage) : storage(newStorage) {
    thread = std::thread([this]() { ThreadWorker::worker(); });
    if (pipe2(addPipeFd, O_NONBLOCK) == -1 || pipe2(transferPipeFd, O_NONBLOCK) == -1) {
        throw std::runtime_error("Error creating pipes");
    }

    fds.push_back({addPipeFd[0], POLLIN, 0});
    fds.push_back({transferPipeFd[0], POLLIN, 0});
}

void ThreadWorker::worker() {
    printf("worker is started\n");
    while (true) {
        int pollResult = poll(fds.data(), fds.size(), -1);
        std::cout << "Success poll ";
        for (auto &el: fds) {
            std::cout << el.fd << " " << el.events << " || ";

        }

        std::cout << std::endl;

        if (pollResult == -1) {
            throw std::runtime_error("poll error");
        }

        handlePipeMessages();

        for (ssize_t i = 2; i < static_cast<ssize_t>(fds.size()); i++) {
            auto &pfd = fds[i];
            if (serverSocketsURI.count(pfd.fd)) {
                if (handleReadDataFromServer(pfd)) {
                    eraseFDByIndex(i);
                }
                continue;
            }

            if (handleClientConnection(pfd)) {
                eraseFDByIndex(i);
            }
        }
    }
}

ssize_t ThreadWorker::eraseFDByIndex(ssize_t &i) {
    fds.erase(fds.begin() + i);
    --i;
    return i;
}

void ThreadWorker::storeClientConnection(int fd, short int events = POLLIN) {
    struct pollfd pfd{};
    pfd.fd = fd;
    pfd.events = events;
    fds.push_back(pfd);
}


bool ThreadWorker::handleClientConnection(pollfd &pfd) {
    if (pfd.revents & POLLIN) {
        return handleClientInput(pfd);
    }

    if (pfd.revents & POLLOUT) {
        return handleClientReceivingResource(pfd);
    }
    return false;
}

void ThreadWorker::addPipe(int writeEnd) {
    if (write(addPipeFd[1], &writeEnd, sizeof(writeEnd)) == -1) {
        throw std::runtime_error("Error writing to addPipe");
    }
}

void ThreadWorker::transferInfo(ClientInfo &info) {
    if (write(transferPipeFd[1], &info, sizeof(info)) == -1) {
        throw std::runtime_error("Error writing to addPipe");
    }
}

void ThreadWorker::storeInfo(ClientInfo& info) {
    storeClientConnection(info.fd, POLLOUT);
    ClientLocalInfo localInfo;
    localInfo.uri = std::move(info.uri);
    localInfo.offset = info.offset;
    clientInfo.insert(std::make_pair(info.fd, localInfo));
}


bool ThreadWorker::handleClientInput(pollfd &pfd) {

    printf("RECEIVE CLIENT INPUT FUNC()\n");
    readClientInput(pfd.fd);

    auto &clientBuf = clientBuffersMap[pfd.fd];
    if (!HttpParser::isHttpRequestComplete(clientBuf)) {
        return false;
    }

    auto req = HttpParser::parseRequest(clientBuf);
    ClientLocalInfo localInfo;
    localInfo.offset = 0;
    localInfo.uri = std::string();
    clientInfo.insert(std::make_pair(pfd.fd, localInfo));
    clientBuffersMap.erase(pfd.fd);

    if (!storage.containsKey(req.uri)) {
        storage.initElement(req.uri);
        int clientFD = pfd.fd;
        pfd.fd = -1;
        auto *cacheElement = storage.getElement(req.uri);
        cacheElement->initReader(clientFD, 0);
        auto serverFD = HostConnector::connectToTargetHost(req);

        printf("add server socket %d\n", serverFD);
        storeClientConnection(serverFD);
        serverSocketsURI.insert(std::make_pair(serverFD, req.uri));
        clientInfo.erase(clientFD);
        return true;
    }

    pfd.events = POLLOUT;
    return false;
}

void ThreadWorker::readClientInput(int fd) {
    char buf[CHUNK_SIZE];
    ssize_t bytesRead = recv(fd, buf, CHUNK_SIZE, 0);
    if (bytesRead < 0) {
        //throw std::runtime_error("recv input from data");
    }

    auto &clientBuf = clientBuffersMap[fd];

    clientBuf += std::string(buf, bytesRead);
}

bool ThreadWorker::handleClientReceivingResource(pollfd &pfd) {
    printf("RECEIVE CLIENT FUNC()\n");
    auto &info = clientInfo.at(pfd.fd);
    auto *cacheElement = storage.getElement(info.uri);

    std::string data = cacheElement->readData(info.offset);

    assert(!data.empty());

    std::cout << "Data read from client " << pfd.fd << std::endl;
    ssize_t bytesSend = send(pfd.fd, data.data(), data.size(), 0);

    if(cacheElement->isFinishReading(info.offset) || bytesSend == -1){
        if(bytesSend == -1){
            fprintf(stderr, "ERROR in %s %s\n", __func__, strerror(errno));
        }

        close(pfd.fd);
        clientInfo.erase(pfd.fd);
        return true;
    }

    info.offset += static_cast<ssize_t>(data.size());

    return false;
}

bool ThreadWorker::handleReadDataFromServer(pollfd &pfd) {
    printf("SERVER DOWNLOAD\n");
    auto uri = serverSocketsURI.at(pfd.fd);
    auto *cacheElement = storage.getElement(uri);

    char buf[CHUNK_SIZE] = {'\0'};
    ssize_t bytesRead = recv(pfd.fd, buf, CHUNK_SIZE, 0);

    cacheElement->appendData(std::string(buf, bytesRead));
    cacheElement->makeReadersReadyToWrite(uri);

    if (bytesRead == 0) {
        printf("MARK IS FINISHED\n");
        cacheElement->markFinished();

        close(pfd.fd);
        return true;
    }

    printf("Bytes read %zd\n", bytesRead);
    return false;
}

void ThreadWorker::handlePipeMessages() {
    int fd;
    ClientInfo info;
    if (fds[0].revents & POLLIN) {
        while (read(addPipeFd[0], &fd, sizeof(fd)) != -1) {
            storeClientConnection(fd);
        }
    }

    if (fds[1].revents & POLLIN) {
        while (read(transferPipeFd[0], &info, sizeof(info)) != -1) {
            transferInfo(info);
        }
    }
}


