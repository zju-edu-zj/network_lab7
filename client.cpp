#include "client.h"

using namespace std;

Client::Client()
{
    sock = -1;
    // TODO: ftok fail
    key_t key = ftok("/tmp", 'A');
    // TODO: msgget fail
    msgQueue = msgget(key, 0600 | IPC_CREAT);
    status = IDLE;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
}

Client::~Client()
{
    // TODO: close fail
    if (sock > 0)
    {
        close(sock);
    }
    // TODO: msgctl fail
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
                // TODO: socket fail
                sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

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

                // TODO: connect fail
                int error = connect(sock, (struct sockaddr *)&server, sizeof(server));
                if (error < 0)
                {
                    throw "connection failed";
                }

                // TODO: pthread_create fail
                pthread_create(&recvThread, nullptr, threadReceiveFunc, this);

                status = CONNECTED;

                printGuideInfo("Connection established.");
            }
            else if (command == "close" && status != IDLE)
            {
                Request req(RequestClose);
                char *packToSend = nullptr;
                int len = req.Serialize(&packToSend);
                send(sock, packToSend, len, 0);

                pthread_cancel(recvThread);
                close(sock);

                sock = -1;
                status = IDLE;

                printGuideInfo("Connection closed.");
            }
            else if (command == "showTime" && status != IDLE)
            {
                Request req(RequestTime);
                char *packToSend = nullptr;
                int len = req.Serialize(&packToSend);
                send(sock, packToSend, len, 0);
                IPCMessage ipcm;
                msgrcv(msgQueue, &ipcm, BUF_SIZE, RequestTime + 10, 0);
                Response res;
                res.Deserialize(ipcm.text, BUF_SIZE);
                printMessage("Time: " + to_string(res.getTime()));
            }
            else if (command == "showServerName" && status != IDLE)
            {
                Request req(RequestName);
                char *packToSend = nullptr;
                int len = req.Serialize(&packToSend);
                send(sock, packToSend, len, 0);
                IPCMessage ipcm;
                msgrcv(msgQueue, &ipcm, BUF_SIZE, RequestName + 10, 0);
                Response res;
                res.Deserialize(ipcm.text, BUF_SIZE);
                printMessage("Server Name:" + res.getString());
            }
            else if (command == "showClientList" && status != IDLE)
            {
                Request req(RequestList);
                char *packToSend = nullptr;
                int len = req.Serialize(&packToSend);
                send(sock, packToSend, len, 0);
                IPCMessage ipcm;
                msgrcv(msgQueue, &ipcm, BUF_SIZE, RequestList + 10, 0);
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
                cin >> mess;
                Request req(RequestMess, mess.length(), mess.c_str(), id);
                char *packToSend = nullptr;
                int len = req.Serialize(&packToSend);
                send(sock, packToSend, len, 0);

                // TODO: 接受返回信号
                IPCMessage ipcm;
                msgrcv(msgQueue, &ipcm, BUF_SIZE, ResponseBack_Fail + 10, 0);
                Response res;
                res.Deserialize(ipcm.text, BUF_SIZE);
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
                    pthread_cancel(recvThread);
                    close(sock);
                    sock = -1;
                    status = IDLE;
                    printGuideInfo("Connection closed.");
                }
                break;
            }
            else
            {
                throw("command not exist");
            }

            IPCMessage ipcm;
            if (msgrcv(msgQueue, &ipcm, BUF_SIZE, ResponseIndicator + 10, IPC_NOWAIT) > 0)
            {
                Response res;
                res.Deserialize(ipcm.text, BUF_SIZE);
                printMessage(res.getString());
            }
        }
        catch (char const *error_message)
        {
            cout << SET_RED_COLOR << error_message << endl
                 << RESET_COLOR;
        }
    }
}

void *threadReceiveFunc(void *client)
{
    Client c = *((Client *)client);
    char buffer[BUF_SIZE];
    IPCMessage ipcm;
    while (1)
    {
        recv(c.sock, buffer, BUF_SIZE, 0);
        int type = Response::getType(buffer);
        int len = Response::getLen(buffer);
        if (type == ResponseBack_Succ)
        {
            ipcm.type = ResponseBack_Fail + 10;
        }
        else
        {
            ipcm.type = type + 10;
        }
        memcpy(ipcm.text, buffer, BUF_SIZE);
        msgsnd(c.msgQueue, &ipcm, len, 0);
    }
}

void printGuideInfo(string info)
{
    cout << SET_GREEN_COLOR
         << info << endl
         << RESET_COLOR;
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
    cout << SET_BLUE_COLOR << info << endl
         << RESET_COLOR;
}

int main(int argc, char **argv)
{
    Client client;
    client.start();
}