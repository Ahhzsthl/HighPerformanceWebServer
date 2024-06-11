#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <sys/types.h>
#include <sys/uio.h>     // readv/writev
#include <arpa/inet.h>   // sockaddr_in
#include <stdlib.h>      // atoi()
#include <errno.h>      

#include "../pool/sqlconnRAII.h"
#include "../buffer/buffer.h"
#include "httprequest.h"
#include "httpresponse.h"

class HttpConn {
public : 
    HttpConn();
    ~HttpConn();

    void Init(int sockFd, const sockaddr_in& addr);
    void Close();
    
    int GetFd() const;
    int GetPort() const;
    const char* GetIP() const;
    sockaddr_in GetAddr() const;

    ssize_t read(int* ErrorNo);             //读的接口，将文件描述符的内容读到读缓冲区
    ssize_t write(int* ErrorNo);            //写的接口，将写缓冲区的内容写到文件描述符中
    bool process();                         //读写之前的初始化操作
    
    int ToWriteBytes() {
        return iov_[0].iov_len + iov_[1].iov_len;
    }
    bool IsKeepAlive() const {
        return request_.IsKeepAlive();
    }

    static bool isET;
    static const char* srcDir;
    static std::atomic<int> userCount;

private :
    int fd_;
    struct sockaddr_in addr_;
    bool isClose_;

    int iovCount_;          //iov_实际使用的缓冲区数目
    struct iovec iov_[2];   //写的缓冲区，可以定义多个缓冲区的地址和大小，用于实现分散读和聚集写
    Buffer readBuff_;       //读缓冲区，从文件描述符读到的内容，对应的是request要解析的数据
    Buffer writeBuff_;       //写缓冲区，对应的是response响应存放的数据，将作为iov_的缓冲区用于，writev

    HttpRequest request_;
    HttpResponse response_;
};

#endif