#include "client.h"

using namespace std;

Client::Client()
{
    sock = -1;
    key_t key = ftok("/tmp", 'A');
    // if (key < 0)
    //     throw "ftok error";
    // {
    // }
    msgQueue = msgget(key, 0600 | IPC_CREAT);
    // if (msgQueue < 0)
    // {
    //     throw "msgget error";
    // }
    status = IDLE;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
}

Client::~Client()
{
    if (sock > 0)
    {
        close(sock);
    }
    msgctl(msgQueue, IPC_RMID, nullptr);
}

void Client::start()
{
    while (1)
    {
        try
        {
            printGuideInfo(status);
            string command;
            cin >> command;

            if (command == "connect" && status == IDLE)
            {
                sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
                if (sock < 0)
                {
                    throw "socket error";
                }

                string server_ip;
                string server_port;

                printGuideInfo("Enter server ip:");
                cin >> server_ip;
                printGuideInfo("Enter server port:");
                cin >> server_port;
                if (server_ip.compare("d") == 0)
                {
                    server_ip = "127.0.0.1";
                }
                if (server_port.compare("d") == 0)
                {
                    server_port = "4234";
                }
                server.sin_addr.s_addr = inet_addr(server_ip.c_str());
                server.sin_port = htons(atoi(server_port.c_str()));

                int c = connect(sock, (struct sockaddr *)&server, sizeof(server));
                if (c < 0)
                {
                    throw "connection failed";
                }

                int p = pthread_create(&recvThread, nullptr, threadReceiveFunc, this);
                if (p != 0)
                {
                    throw "pthread_create error";
                }

                status = CONNECTED;

                printGuideInfo("Connection established.");
            }
            else if (command == "close" && status != IDLE)
            {
                Request req(RequestClose);
                char *packToSend = nullptr;
                int len = req.Serialize(&packToSend);
                int s = send(sock, packToSend, len, 0);
                if (s < 0)
                {
                    throw "send error";
                }

                int p = pthread_cancel(recvThread);
                if (p != 0)
                {
                    throw "pthread_cancel error";
                }
                int c = close(sock);
                if (c < 0)
                {
                    throw "close sock error";
                }

                sock = -1;
                status = IDLE;

                printGuideInfo("Connection closed.");
            }
            else if (command == "showTime" && status != IDLE)
            {
                Request req(RequestTime);
                char *packToSend = nullptr;
                int len = req.Serialize(&packToSend);
                for (int i = 0; i < 100; i++)
                {
                    int s = send(sock, packToSend, len, 0);
                    if (s < 0)
                    {
                        throw "send error";
                    }
                }
                for (int i = 0; i < 100; i++)
                {
                    IPCMessage ipcm;
                    int rcv = msgrcv(msgQueue, &ipcm, BUF_SIZE, RequestTime + 10, 0);
                    if (rcv < 0)
                    {
                        throw "msgrcv error";
                    }
                    Response res;
                    res.Deserialize(ipcm.text, BUF_SIZE);
                    printMessage("Count: " + to_string(i) + "Time: " + to_string(res.getTime()));
                }
            }
            else if (command == "showServerName" && status != IDLE)
            {
                Request req(RequestName);
                char *packToSend = nullptr;
                int len = req.Serialize(&packToSend);
                int s = send(sock, packToSend, len, 0);
                if (s < 0)
                {
                    throw "send error";
                }
                IPCMessage ipcm;
                int rcv = msgrcv(msgQueue, &ipcm, BUF_SIZE, RequestName + 10, 0);
                if (rcv < 0)
                {
                    throw "msgrcv error";
                }
                Response res;
                res.Deserialize(ipcm.text, BUF_SIZE);
                printMessage("Server Name:" + res.getString());
            }
            else if (command == "showClientList" && status != IDLE)
            {
                Request req(RequestList);
                char *packToSend = nullptr;
                int len = req.Serialize(&packToSend);
                int s = send(sock, packToSend, len, 0);
                if (s < 0)
                {
                    throw "send error";
                }
                IPCMessage ipcm;
                int rcv = msgrcv(msgQueue, &ipcm, BUF_SIZE, RequestList + 10, 0);
                if (rcv < 0)
                {
                    throw "msgrcv error";
                }
                printMessage("Client list:");
                Response res;
                res.Deserialize(ipcm.text, BUF_SIZE);
                for (Client_info i : res.getList())
                {
                    printMessage("Id: " + to_string(i.id) + " IP: " + i.ip_port);
                }
                status = INFORMED;
            }
            else if (command == "send" && status == INFORMED)
            {
                int id;
                string mess;
                printGuideInfo("Enter destination client ID:");
                cin >> id;
                printGuideInfo("Enter message to sent (end with \\n):");
                getline(cin, mess); // ignore newline
                getline(cin, mess);
                Request req(RequestMess, mess.length(), mess.c_str(), id);
                char *packToSend = nullptr;
                int len = req.Serialize(&packToSend);
                int s = send(sock, packToSend, len, 0);
                if (s < 0)
                {
                    throw "send error";
                }

                // TODO: 接受返回信号
                IPCMessage ipcm;
                int rcv = msgrcv(msgQueue, &ipcm, BUF_SIZE, ResponseBack_Fail + 10, 0);
                printMessage("type1: " + to_string(ipcm.type - 10));
                if (rcv < 0)
                {
                    throw "msgrcv error";
                }
                Response res;
                res.Deserialize(ipcm.text, BUF_SIZE);
                printMessage("type2: " + to_string(res.type));
                if (res.type == ResponseBack_Succ)
                {
                    printGuideInfo("Message succeed to send.");
                }
                else
                {
                    printGuideInfo("Message fail to send.");
                }
            }
            else if (command == "quit")
            {
                if (status != IDLE)
                {
                    int p = pthread_cancel(recvThread);
                    if (p != 0)
                    {
                        throw "pthread_cancel error";
                    }
                    int c = close(sock);
                    if (c < 0)
                    {
                        throw "close socket error";
                    }
                    sock = -1;
                    status = IDLE;
                    printGuideInfo("Connection closed.");
                }
                break;
            }
            else if (command == "receive")
            {
                IPCMessage ipcm;
                int rcv = msgrcv(msgQueue, &ipcm, BUF_SIZE, ResponseIndicator + 10, IPC_NOWAIT);
                if (rcv > 0)
                {
                    Response res;
                    res.Deserialize(ipcm.text, BUF_SIZE);
                    printMessage(res.getString());
                }
            }
            else
            {
                throw("command not exist");
            }
        }
        catch (char const *error_message)
        {
            cout << SET_RED_COLOR << error_message << RESET_COLOR << endl;
        }
    }
}

void *threadReceiveFunc(void *client)
{
    Client c = *((Client *)client);
    char buffer[BUF_SIZE];
    while (1)
    {
        try
        {
            memset(buffer, 0, BUF_SIZE);
            int r = recv(c.sock, buffer, BUF_SIZE, 0);
            if (r < 0)
            {
                throw "recv error";
            }
            char *cur = buffer;
            while (1)
            {
                IPCMessage ipcm;
                memset(ipcm.text, 0, BUF_SIZE);
                unsigned int type = Response::getType(cur);
                unsigned int len = Response::getLen(cur);
                // printMessage("type: " + to_string(type));
                // printMessage("len: " + to_string(len));
                if (type < RequestTime || type > ResponseIndicator || len < 12)
                {
                    break;
                }
                // for (int i = 12; i < len; i++)
                // {
                //     cout << cur[i];
                // }
                // cout << endl;
                if (type == ResponseBack_Succ)
                {
                    ipcm.type = ResponseBack_Fail + 10;
                }
                else
                {
                    ipcm.type = type + 10;
                }
                memcpy(ipcm.text, cur, BUF_SIZE);
                int m = msgsnd(c.msgQueue, &ipcm, BUF_SIZE, 0);
                if (m < 0)
                {
                    throw "msgsnd error";
                }
                cur += len;
            }
        }
        catch (char const *error_message)
        {
            cout << SET_RED_COLOR << error_message << RESET_COLOR << endl;
        }
    }
}

void printGuideInfo(string info)
{
    cout << SET_GREEN_COLOR
         << info << RESET_COLOR << endl;
}

void printGuideInfo(STATUS status)
{
    if (status == IDLE)
    {
        printGuideInfo("Enter your command:\n- connect\n- quit");
    }
    else if (status == CONNECTED)
    {
        printGuideInfo("Enter your command:\n- connect\n- close\n- showTime\n- showServerName\n- showClientList\n- quit");
    }
    else if (status == INFORMED)
    {
        printGuideInfo("Enter your command:\n- connect\n- close\n- showTime\n- showServerName\n- showClientList\n- send\n- quit");
    }
}

void printMessage(string info)
{
    cout << SET_BLUE_COLOR << info << RESET_COLOR << endl;
}

int main(int argc, char **argv)
{
    Client client;
    client.start();
}