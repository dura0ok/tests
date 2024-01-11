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
    if (pipe2(addPipeFd, O_NONBLOCK) == -1 || pipe2(removePipeFd, O_NONBLOCK) == -1) {
        throw std::runtime_error("Error creating pipes");
    }

    fds.push_back({addPipeFd[0], POLLIN, 0});
    fds.push_back({removePipeFd[0], POLLIN, 0});
}

void ThreadWorker::worker() {
    printf("worker is started\n");
    while (true) {
        std::cout << "Success poll ";
        for(auto &el : fds){
            std::cout  << el.fd << " " << el.events << " || ";

        }
        int pollResult = poll(fds.data(), fds.size(), -1);

        std::cout << std::endl;

        if (pollResult == -1) {
            throw std::runtime_error("poll error");
        }

        handlePipeMessages();

        for (size_t i = 2; i < fds.size(); i++) {
            auto &pfd = fds[i];
            if (serverSocketsURI.count(pfd.fd)) {
                handleReadDataFromServer(pfd);
                continue;
            }

            handleClientConnection(pfd);
        }
    }
}

void ThreadWorker::storeClientConnection(int fd) {
    struct pollfd pfd{};
    pfd.fd = fd;
    pfd.events = POLLIN;
    fds.push_back(pfd);
}

void ThreadWorker::removeClientConnection(int fd) {
    fds.erase(std::remove_if(fds.begin(), fds.end(),
                                     [fd](const pollfd& p) {
                                         return p.fd == fd;
                                     }), fds.end());
}

void ThreadWorker::handleClientConnection(pollfd &pfd) {
    if (pfd.revents & POLLIN) {
        handleClientInput(pfd);
        return;
    }

    if (pfd.revents & POLLOUT) {
        handleClientReceivingResource(pfd);
        return;
    }
}

void ThreadWorker::addPipe(int writeEnd) {
    if (write(addPipeFd[1], &writeEnd, sizeof(writeEnd)) == -1) {
        throw std::runtime_error("Error writing to addPipe");
    }
}

void ThreadWorker::removePipe(int writeEnd) {
    if (write(removePipeFd[1], &writeEnd, sizeof(writeEnd)) == -1) {
        throw std::runtime_error("Error writing to removePipe");
    }
}


void ThreadWorker::handleClientInput(pollfd &pfd) {

    printf("RECEIVE CLIENT INPUT FUNC()\n");
    readClientInput(pfd.fd);

    auto &clientBuf = clientBuffersMap[pfd.fd];
    if (!HttpParser::isHttpRequestComplete(clientBuf)) {
        return;
    }

    auto req = HttpParser::parseRequest(clientBuf);

    clientSocketsURI.insert(std::make_pair(pfd.fd, req.uri));

    if (!storage.containsKey(req.uri)) {
        storage.initElement(req.uri);
        auto serverFD = HostConnector::connectToTargetHost(req);
        printf("add server socket %d\n", serverFD);
        storeClientConnection(serverFD);
        serverSocketsURI.insert(std::make_pair(serverFD, req.uri));
        clientBuffersMap.insert(std::pair(serverFD, std::string()));
    }


    //pfd.events = POLLOUT;
    auto *cacheElement = storage.getElement(req.uri);
    cacheElement->initReader(pfd.fd);
    clientBuf.clear();
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

void ThreadWorker::handleClientReceivingResource(pollfd &pfd) {
    printf("RECEIVE CLIENT FUNC()\n");
    auto uri = clientSocketsURI.at(pfd.fd);
    auto *cacheElement = storage.getElement(uri);



    auto data = cacheElement->readData(pfd.fd);
    std::cout << "Data read from client " << pfd.fd << std::endl;
    ssize_t bytesSend = send(pfd.fd, data.data(), data.size(), 0);
    if (bytesSend == -1 && errno == EPIPE) {
        removePipe(pfd.fd);
    }

    if (cacheElement->isFinishReading(pfd.fd)) {
        printf("FINISH READING\n");
        removePipe(pfd.fd);
        return;
    }

    if(data.empty()){
        printf("EMPTY DATA in receive\n");
        pfd.events &= ~POLLOUT;
    }
}

void ThreadWorker::handleReadDataFromServer(pollfd &pfd) {
    printf("SERVER DOWNLOAD\n");
    auto uri = serverSocketsURI.at(pfd.fd);
    auto *cacheElement = storage.getElement(uri);
    auto &bufToReceiveStatusCode = clientBuffersMap.at(pfd.fd);

    if (cacheElement->isFinished()) {

        auto code = HttpParser::parseStatusCode(bufToReceiveStatusCode);

        if (code < 0) {
            printf("WHY YOU DO THIS ?\n");
            return;
        }

        size_t readersCount = cacheElement->getReadersCount();
        printf("CACHE ELEMENT IS FINISHED %d %zu\n", code, readersCount);

        //bufToReceiveStatusCode.clear();
        removeClientConnection(pfd.fd);
        return;
    }

    char buf[CHUNK_SIZE] = {'\0'};
    ssize_t bytesRead = recv(pfd.fd, buf, CHUNK_SIZE, 0);
    cacheElement->appendData(std::string(buf, bytesRead));

    cacheElement->makeReadersReadyToWrite(fds);

    if (!HttpParser::isStatusCodeReceived(bufToReceiveStatusCode)) {
        bufToReceiveStatusCode += std::string(buf, bytesRead);
    }

    if (bytesRead == 0) {
        printf("MARK IS FINISHED\n");
        cacheElement->markFinished();
        return;
    }

    printf("Bytes read %zd\n", bytesRead);
}

void ThreadWorker::handlePipeMessages() {
    int fd;
    if (fds[0].revents & POLLIN) {
        while (read(addPipeFd[0], &fd, sizeof(fd)) != -1) {
            storeClientConnection(fd);
        }
    }

    if (fds[1].revents & POLLIN) {
        while (read(removePipeFd[0], &fd, sizeof(fd)) != -1) {
            removeClientConnection(fd);
        }
    }
}


