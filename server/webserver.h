#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <unordered_map>
#include <fcntl.h>       // fcntl()
#include <unistd.h>      // close()
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "epoller.h"
#include "../pool/sqlconnpool.h"
#include "../pool/threadpool.h"
#include "../pool/sqlconnRAII.h"
#include "../httpconn/httpconn.h"

class WebServer {
public : 
    WebServer(
        int port, int trigeMode, int timeoutMS, bool OptLinger, 
        int sqlPort, const char* sqlUser, const char* sqlPwd,
        const char* dbName, int connPoolNum, int threadNum);
    ~WebServer();
    void start();                                   //开始监听的接口，死循环监听处理事件

private :
    bool InitSocket_();                             //初始化监听的fd，关联到epoller
    void InitEventMode_(int trigeMode);             //初始化事件触发模式，ET LT

    void DealListen_();                             //有新的用户进来，增加监听的fd
    void DealWrite_(HttpConn* client);              //处理用户write的事件
    void DealRead_(HttpConn* client);               //处理用户read的事件

    void CloseConn_(HttpConn* client);              //关闭用户连接
    void AddClient_(int fd, sockaddr_in addr);      //添加监听的用户
    void SendError_(int fd, const char* info);      //处理用户超出上限
    // void ExtentTime_(HttpConn* client);             //超时处理

    void OnRead_(HttpConn* client);
    void OnWrite_(HttpConn* client);
    void OnProcess(HttpConn* client);

    int port_;
    bool openLinger_;       //优雅关闭
    int timeoutMS_;         //定时器的超时处理时间
    bool isClose_;
    int listenFd_;
    char* srcDir_;

    uint32_t listenEvent_;  //
    uint32_t connEvent_;    //

    std::unique_ptr<ThreadPool> threadpool_;
    std::unique_ptr<Epoller> epoller_;
    std::unordered_map<int, HttpConn> users_;

    static const int MAX_FD = 65536;
    static int SetFdNonblock(int fd);
};

#endif