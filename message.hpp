#pragma once
#include <cstring>
#include <cassert>
#include <string>
#include <vector>

#define RequestTime 0
#define RequestName 1
#define RequestList 2
#define RequestMess 3
#define RequestClose 4
#define ResponseBack_Fail 5 // error, response back directly
#define ResponseBack_Succ 6 // success, response back directly
#define ResponseIndicator 7 // from other clent

struct packet
{
    int length;
    int type;
    int num;
    char message[0];
};
using namespace std;
class Client_info
{
public:
    Client_info()
    {
    }
    Client_info(int _id, string _ip_port) : id(_id), ip_port(_ip_port)
    {
        ip_len = ip_port.size();
    }
    // addr has already existed
    int Serialize(char *addr)
    {
        *(int *)addr = id;
        int offset = sizeof(int);
        *(int *)(addr + offset) = ip_len;
        offset += sizeof(int);
        memcpy(addr + offset, ip_port.c_str(), ip_port.size());
        offset += ip_port.size();
        return offset;
    }
    int getLength()
    {
        return 2 * sizeof(int) + ip_port.size();
    }
    int Deserialize(char *addr)
    {
        id = *(int *)addr;
        int offset = sizeof(int);
        ip_len = *(int *)(addr + offset);
        offset += sizeof(int);
        ip_port = string(addr + offset, ip_len);
        offset += ip_port.size();
        return offset;
    }
    int id;
    int ip_len;
    string ip_port; // of the form "xxx.xx.xx.xx:1324"
};
class Request
{
public:
    Request() {}
    Request(int _type)
    {
        if (_type >= 0 && _type < 3)
        {
            request_packet = (struct packet *)new char[sizeof(struct packet)];
            request_packet->type = _type;
            request_packet->length = sizeof(struct packet);
        }
    }
    Request(int _type, int message_length, const char *bytes, int _num = 0)
    {
        assert(_type == RequestMess);
        length = sizeof(struct packet) + message_length;
        request_packet = (struct packet *)new char[length];
        request_packet->type = _type;
        request_packet->length = length;
        request_packet->num = _num;
        memcpy(request_packet->message, bytes, message_length);
    }
    ~Request()
    {
        if (request_packet)
        {
            delete request_packet;
        }
    }
    /*
     * send_adr will be set to the data address to be transmitted and return the total size
     */
    int Serialize(char **send_adr)
    {
        *send_adr = (char *)request_packet;
        return length;
    }
    /*
     * construct a request object from the rev_addr and if the packet length is bigger than maxsize(buffer size),
     * you will have to receive data again and reset rev_addr
     */
    bool Deserialize(char *rev_addr, int maxsize, bool flag = true)
    {
        if (flag)
        {
            request_packet = (struct packet *)rev_addr;
            length = request_packet->length;
            request_packet = (struct packet *)new char[length];
            if (length > maxsize)
            {
                memcpy(request_packet, rev_addr, maxsize);
                offset = maxsize;
                return false;
            }
            else
            {
                memcpy(request_packet, rev_addr, length);
                return true;
            }
        }
        else
        {
            int remain = length - offset;
            int rev_len = (maxsize > remain) ? remain : maxsize;
            memcpy((char *)request_packet + offset, rev_addr, rev_len);
            offset += rev_len;
            if (rev_len == maxsize)
            {
                return false;
            }
            else
            {
                return true;
            }
        }
    }

    struct packet *request_packet = nullptr;
    int length = sizeof(struct packet);

private:
    int offset = 0;
};

/*
 * we should first denote the type and then call the corresponding method to set it
 */
class Response
{
public:
    Response()
    {
    }
    Response(int _type)
    {
        type = _type;
    }
    ~Response()
    {
        if (response_packet)
        {
            delete response_packet;
        }
    }
    /*
     * directly get the type of response if the send_adr is the head of a packet
     */
    static int getType(char *send_adr)
    {
        return ((struct packet *)send_adr)->type;
    }

    void setTime(long cur_time)
    {
        assert(type == RequestTime);
        length = sizeof(struct packet) + sizeof(long);
        response_packet = (struct packet *)new char[length];
        response_packet->type = type;
        response_packet->length = length;
        *(long *)response_packet->message = cur_time;
    }
    long getTime()
    {
        return *(long *)response_packet->message;
    }
    /*
     * for name and message service
     */
    void setString(const char *message)
    {
        length = sizeof(struct packet) + strlen(message);
        response_packet = (struct packet *)new char[length]; // note for an extra 0
        response_packet->type = type;
        response_packet->length = length;
        memcpy(response_packet->message, message, strlen(message));
    }
    /*
     * for name and message service
     * actually you can directly operate response_packet if you think string is too tedious
     */
    std::string getString()
    {
        return std::string(response_packet->message, length - sizeof(struct packet));
    }

    void setList(vector<Client_info> &clients)
    {
        length = sizeof(struct packet);
        for (auto client : clients)
        {
            length += client.getLength();
        }
        response_packet = (struct packet *)new char[length]; // note for an extra 0
        response_packet->type = type;
        response_packet->length = length;
        int _offset = 0;
        for (auto client : clients)
        {
            _offset += client.Serialize(response_packet->message + _offset);
        }
    }
    vector<Client_info> getList()
    {
        vector<Client_info> clients;
        int _offset = 0;
        while (_offset < length - sizeof(struct packet))
        {
            Client_info client;
            _offset += client.Deserialize(response_packet->message + _offset);
            clients.push_back(client);
        }
        return clients;
    }
    /*
     * send_adr will be set to the data address to be transmitted and return the total size
     */
    int Serialize(char **send_adr)
    {
        *send_adr = (char *)response_packet;
        return length; // for an extra 0
    }
    /*
     * construct a response object from the rev_addr and if the packet length is bigger than maxsize(buffer size),
     * you will have to receive data again and reset rev_addr
     */
    bool Deserialize(char *rev_addr, int maxsize, bool flag = true)
    {
        if (flag)
        {
            response_packet = (struct packet *)rev_addr;
            length = response_packet->length;
            type = response_packet->type;
            // num = response_packet->type;
            response_packet = (struct packet *)new char[length];
            if (length > maxsize)
            {
                memcpy(response_packet, rev_addr, maxsize);
                offset = maxsize;
                return false;
            }
            else
            {
                memcpy(response_packet, rev_addr, length);
                return true;
            }
        }
        else
        {
            int remain = length - offset;
            int rev_len = (maxsize > remain) ? remain : maxsize;
            memcpy((char *)response_packet + offset, rev_addr, rev_len);
            offset += rev_len;
            if (rev_len == maxsize)
            {
                return false;
            }
            else
            {
                return true;
            }
        }
    }
    struct packet *response_packet = nullptr;
    int length = sizeof(struct packet);
    int type;

private:
    int offset = 0;
};