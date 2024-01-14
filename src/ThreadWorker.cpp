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
    //printf("worker is started\n");
    while (true) {
        std::cout << "Success poll ";
        for (auto &el: fds) {
            std::cout << el.fd << " " << el.events << " || ";

        }

        std::cout << std::endl;
        int pollResult = poll(fds.data(), fds.size(), -1);


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

void ThreadWorker::transferInfo(ClientInfo *info) {
    if (write(transferPipeFd[1], &info, sizeof(ClientInfo *)) == -1) {
        throw std::runtime_error("Error writing to addPipe");
    }
}

void ThreadWorker::storeInfo(ClientInfo *info) {
    storeClientConnection(info->fd, POLLOUT);
    clientInfo.insert(std::make_pair(info->fd, info));
}

void sendHTTP500Error(int clientSocket) {
    std::string response = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n";

    if (send(clientSocket, response.c_str(), response.size(), 0) == -1) {
        std::cerr << "Failed to send HTTP 500 response to the socket" << std::endl;
    }
}

bool ThreadWorker::handleClientInput(pollfd &pfd) {
    //printf("RECEIVE CLIENT INPUT FUNC()\n");
    if(readClientInput(pfd.fd)){
        return true;
    }

    auto &clientBuf = clientBuffersMap[pfd.fd];

    if (!HttpParser::isHttpRequestComplete(clientBuf)) {
        return false;
    }

    httpparser::Request req;
    try{
        req = HttpParser::parseRequest(clientBuf);
    }catch(...){
        sendHTTP500Error(pfd.fd);
        clientBuffersMap.erase(pfd.fd);
        close(pfd.fd);
        return true;
    }


    clientBuffersMap.erase(pfd.fd);

    auto *info = new ClientInfo();
    info->uri = req.uri;
    info->fd = pfd.fd;
    info->offset = 0;
    clientInfo.insert(std::make_pair(pfd.fd, info));

    if (!storage.containsKey(req.uri)) {
        storage.initElement(req.uri);
        int clientFD = pfd.fd;
        pfd.fd = -1;
        auto *cacheElement = storage.getElement(req.uri);
        cacheElement->incrementReadersCount();
        cacheElement->initReader(info);
        auto serverFD = HostConnector::connectToTargetHost(req);

        printf("ADD server socket %d\n", serverFD);
        storeClientConnection(serverFD);
        serverSocketsURI.insert(std::make_pair(serverFD, req.uri));
        clientInfo.erase(clientFD);
        return true;
    }

    pfd.events = POLLOUT;
    return false;
}

bool ThreadWorker::readClientInput(int fd) {
    char buf[CHUNK_SIZE];
    ssize_t bytesRead = recv(fd, buf, CHUNK_SIZE, 0);
    if (bytesRead < 0) {
        clientBuffersMap.erase(fd);
        close(fd);
        return true;
    }

    auto &clientBuf = clientBuffersMap[fd];

    clientBuf += std::string(buf, bytesRead);
    return false;
}

void ThreadWorker::handleFinishRead(ClientInfo *info, CacheElement *cacheElement, bool closeFD) {
    cacheElement->decrementReadersCount();
    storage.lock();
    if (cacheElement->isReadersEmpty() && cacheElement->getStatusCode() != 200) {
        storage.clearElement(info->uri);
    }
    storage.unlock();
    cleanClientInfo(cacheElement, info, closeFD);
}

bool ThreadWorker::handleClientReceivingResource(pollfd &pfd) {
    //printf("RECEIVE CLIENT FUNC()\n");
    auto &info = clientInfo.at(pfd.fd);
    auto *cacheElement = storage.getElement(info->uri);
    char buf[BUFSIZ];
    auto size = cacheElement->readData(buf, BUFSIZ, info->offset);
    if (size == 0) {
        handleFinishRead(info, cacheElement, cacheElement->isFinishReading(info->offset));
        return true;
    }


    //std::cout << "Data read from client " << pfd.fd << std::endl;
    ssize_t bytesSend = send(pfd.fd, buf, size, 0);

    if (bytesSend == -1 || cacheElement->isFinishReading(info->offset + static_cast<ssize_t>(size))) {
        printf("RECEIVE FINISH READ TEST!!!!!!!!!!\n");
        if (bytesSend == -1) {
            fprintf(stderr, "ERROR in %s %s\n", __func__, strerror(errno));
        }


        handleFinishRead(info, cacheElement, true);
        return true;
    }

    info->offset += static_cast<ssize_t>(size);

    return false;
}

void ThreadWorker::cleanClientInfo(CacheElement *cacheElement, ClientInfo *info, bool closeFD) {
    clientInfo.erase(info->fd);
    if (closeFD) {
        close(info->fd);
        fprintf(stderr, "CLOSING %d %s\n", info->fd, __func__);
        delete info;
    } else {
        cacheElement->initReader(info);
    }
}

bool ThreadWorker::handleReadDataFromServer(pollfd &pfd) {
    //printf("SERVER DOWNLOAD\n");
    auto uri = serverSocketsURI.at(pfd.fd);
    auto *cacheElement = storage.getElement(uri);

    char buf[CHUNK_SIZE] = {'\0'};
    ssize_t bytesRead = recv(pfd.fd, buf, CHUNK_SIZE, 0);

    if (bytesRead < 0) {
        if(errno != EAGAIN){
            fprintf(stderr, "ERROR < 0 %s\n", strerror(errno));
            cacheElement->markFinished();
            close(pfd.fd);
            return true;
        }
        
    }

    cacheElement->appendData(buf, bytesRead);
    cacheElement->makeReadersReadyToWrite();


    if (bytesRead == 0) {
        printf("MARK IS FINISHED %zu\n", cacheElement->getDataSize());
        cacheElement->markFinished();
        serverSocketsURI.clear();
        fprintf(stderr, "CLOSING %d %s\n", pfd.fd, __func__);
        close(pfd.fd);
        return true;
    }

    //printf("Bytes read %zd\n", bytesRead);
    return false;
}

void ThreadWorker::handlePipeMessages() {
    int fd;
    ClientInfo *info;
    if (fds[0].revents & POLLIN) {
        while (read(addPipeFd[0], &fd, sizeof(fd)) != -1) {
            storeClientConnection(fd);
        }
    }

    if (fds[1].revents & POLLIN) {
        while (read(transferPipeFd[0], &info, sizeof(ClientInfo *)) != -1) {
            storeInfo(info);
        }
    }
}


