#ifndef _CLIENT_H_
#define _CLIENT_H_

#include <iostream>
#include <cstring>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <pthread.h>
#include <sys/msg.h>

#include "message.hpp"

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 4234
#define BUF_SIZE 4096
#define SET_BLUE_COLOR "\033[34m"
#define SET_RED_COLOR "\033[31m"
#define SET_GREEN_COLOR "\033[32m"
#define RESET_COLOR "\033[0m"

using namespace std;

enum STATUS
{
    IDLE,
    CONNECTED,
    INFORMED,
};

struct IPCMessage
{
    long type;
    char text[BUF_SIZE];
};

class Client
{
private:
    int sock;
    int msgQueue;
    pthread_t recvThread;
    STATUS status;
    sockaddr_in server;

    friend void *threadReceiveFunc(void *client);

public:
    Client();
    ~Client();
    void start();
};

void printGuideInfo(string info);

void printGuideInfo(STATUS status);

void printMessage(string info);

void *threadReceiveFunc(void *client);

#endif