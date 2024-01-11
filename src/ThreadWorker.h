#pragma once


#include <thread>
#include <vector>
#include <sys/poll.h>
#include "cache/Storage.h"
#include "helpers/HttpParser.h"

class ThreadWorker {
public:
    explicit ThreadWorker(Storage &storage);

    void storeClientConnection(int fd);

    void removePipe(int writeEnd);

private:
    std::vector<pollfd> fds;
    std::thread thread;
    Storage &storage;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t dataMutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t dataCond = PTHREAD_COND_INITIALIZER;

    int addPipeFd[2]{};
    int removePipeFd[2]{};

    void worker();

    std::map<int, std::string> serverSocketsURI;
    std::map<int, std::string> clientSocketsURI;
    std::map<int, std::string> clientBuffersMap;

    void handleClientConnection(pollfd &pfd);

    void handleReadDataFromServer(pollfd &pfd);


    void handleClientInput(pollfd &pfd);

    void handleClientReceivingResource(pollfd &pfd);

    void readClientInput(int fd);

    void addPipe(int writeEnd);

    void handlePipeMessages();

    void applyPendingChanges();
};