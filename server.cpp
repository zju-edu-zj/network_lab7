#include <iostream>
#include <cstdio>
#include <cstring>       // void *memset(void *s, int ch, size_t n);
#include <sys/types.h>   // 数据类型定义
#include <sys/socket.h>  // 提供socket函数及数据结构sockaddr
#include <arpa/inet.h>   // 提供IP地址转换函数，htonl()、htons()...
#include <netinet/in.h>  // 定义数据结构sockaddr_in
#include <pthread.h>
#include <unistd.h>
#include <string>
#include "message.hpp"
#include <time.h>
class thread_data{
    public:
    thread_data(){

    }
    thread_data(int _id,string _ip_port,int sockfd):client_info(_id,_ip_port),client_sockfd(sockfd){
        
    }
    Client_info client_info;
    int client_sockfd;
};
std::vector<thread_data> database;
pthread_rwlock_t rwlock; //rwlock of database
const int PORT = 4234;
const int BufferSize = 1500;
void *serveForClient(void* num);

using namespace std;
int main(){
    int server_sockfd = socket(PF_INET,SOCK_STREAM,IPPROTO_TCP);  
    if(server_sockfd == -1){
        close(server_sockfd);
        perror("socket error!");
    }
    struct sockaddr_in server_addr;
    memset(&server_addr,0,sizeof(server_addr)); // 结构体清零
    server_addr.sin_family = AF_INET;  // 协议
    server_addr.sin_port = htons(PORT);  // 端口16位, 此处不用htons()或者错用成htonl()会连接拒绝!!
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 本地所有IP

    if(bind(server_sockfd,(struct sockaddr*)&server_addr,sizeof(server_addr)) == -1){
        close(server_sockfd);
        perror("bind error");
    }
    // 第二个参数为相应socket可以排队的准备道来的最大连接个数
    if(listen(server_sockfd, 10) == -1){
        close(server_sockfd);
        perror("listen error");
    }
    cout << "Listen on port " << PORT << endl ;

    while(1){
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        // accept()函数从处于established状态的连接队列头部取出一个已经完成的连接，
        // 如果这个队列没有已经完成的连接，accept()函数就会阻塞当前线程，直到取出队列中已完成的客户端连接为止。
        int client_sockfd = accept(server_sockfd, (struct sockaddr*)&client_addr, &client_len);
        //out << "established" << endl;
        unsigned port = client_addr.sin_port;
        port = ntohs(port); //conver to host
        string ip = inet_ntoa(client_addr.sin_addr);
        ip.push_back(':');
        ip += std::to_string(port);

        pthread_rwlock_rdlock(&rwlock);
        int num = database.size();
        pthread_rwlock_unlock(&rwlock);
        thread_data cur_thread(num,ip,client_sockfd); //the client data

        cout << "The connection of client " << num << " which is from " << cur_thread.client_info.ip_port <<  " is established" << endl;

        pthread_rwlock_wrlock(&rwlock);
        database.push_back(cur_thread);
        pthread_rwlock_unlock(&rwlock);
        pthread_t pt;
        pthread_create(&pt, NULL, serveForClient, (void*)num);
    }

}

void *serveForClient(void* num_ptr){
    long num = (long)num_ptr;  //directly convert pointer to int, which is the serial number in vector
    pthread_rwlock_rdlock(&rwlock);
    thread_data cur_thread = database[num];
    pthread_rwlock_unlock(&rwlock);
    int clientfd = cur_thread.client_sockfd;
    char recvbuf[BufferSize];
    char sendbuf[BufferSize] = "recv successfully.\n";
    while(1){
        memset(recvbuf,0,sizeof(recvbuf));  //set to zero to avoid some strange situations
        int stat = recv(clientfd, recvbuf, sizeof(recvbuf), 0);
        if(stat<0){
            perror("send error!");
            continue;
        }else if(stat==0){ //the connection is closed manually by client
            pthread_rwlock_wrlock(&rwlock);
            database.erase(database.begin()+num);
            pthread_rwlock_unlock(&rwlock);
            break;  //the thread is dropped
        }
        Request req;
        req.Deserialize(recvbuf,BufferSize);
        switch(req.request_packet->type){
            case RequestTime:
            {
                Response resp(RequestTime);
                resp.setTime(time(NULL));
                char* send_adr;
                int length = resp.Serialize(&send_adr);
                send(clientfd, send_adr,length, 0);
            }
            break;
            case RequestName:
            {
                Response resp(RequestName);
                char name[256];
	            gethostname(name, sizeof(name));
                resp.setString(name);
                char* send_adr;
                int length = resp.Serialize(&send_adr);
                send(clientfd, send_adr,length, 0);
            }
            break;
            case RequestList:
            {
                Response resp(RequestList);
                pthread_rwlock_rdlock(&rwlock);
                vector<Client_info> clients;
                clients.reserve(database.size()+1);
                for(auto cthread:database){
                    clients.push_back(cthread.client_info);
                }
                pthread_rwlock_unlock(&rwlock);
                resp.setList(clients);
                char* send_adr;
                int length = resp.Serialize(&send_adr);
                send(clientfd, send_adr,length, 0);
            }
            break;
            case RequestMess:
            {
                int forwarded_num = req.request_packet->num;
                pthread_rwlock_rdlock(&rwlock);
                int database_size = database.size();  //read now in case other threads modify it
                pthread_rwlock_unlock(&rwlock);
                if(forwarded_num<0 || forwarded_num >= database_size){
                    char error_m[] = "The client doesn't exist\n";
                    Response resp(ResponseBack_Fail);
                    resp.setString(error_m);
                    char* send_adr;
                    int length = resp.Serialize(&send_adr);
                    send(clientfd, send_adr,length, 0);
                }else{  //The client exist

                    //forward indicator message
                    Response resp(ResponseIndicator);
                    resp.setString(req.request_packet->message);
                    //get the fd of the forwarded client
                    pthread_rwlock_rdlock(&rwlock);
                    int forwarded_fd = database[forwarded_num].client_sockfd;
                    pthread_rwlock_unlock(&rwlock);
                    char* send_adr;
                    int length = resp.Serialize(&send_adr);
                    send(forwarded_fd, send_adr,length, 0);

                    //next send back successful message
                    char succ_m[] = "Forwarding succeed!\n";
                    Response resp_back(ResponseBack_Succ);
                    resp_back.setString(succ_m);
                    //char* send_adr;
                    length = resp.Serialize(&send_adr);
                    send(clientfd, send_adr,length, 0);
                }
            }
            break;
            default:
            break;
        }
        fputs(recvbuf,stdout);
        strcpy(sendbuf, recvbuf);
        if(strcmp(recvbuf,"exit\n") == 0){
            send(clientfd, "connection close.\n", sizeof("connection close.\n"), 0);
            break;
        }
        send(clientfd, sendbuf, sizeof(sendbuf), 0);
        memset(recvbuf,0,sizeof(recvbuf));
    }
    cout << "The connection of client " << num << " which is from " << cur_thread.client_info.ip_port <<  " is dropped" << endl;
    cout << "......................." << endl;
    //cout << "here" << num;
}