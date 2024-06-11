#include "httpconn.h"
using namespace std;

const char* HttpConn::srcDir;
std::atomic<int> HttpConn::userCount;
bool HttpConn::isET;

HttpConn::HttpConn() {
    fd_ = -1;
    addr_ = {0};
    isClose_ = true;
}

HttpConn::~HttpConn() {
    Close();
}

void HttpConn::Init(int fd, const sockaddr_in& addr) {
    assert(fd > 0);
    userCount++;            //atomic类型变量，
    addr_ = addr;
    fd_ = fd;
    writeBuff_.RetriveAll();
    readBuff_.RetriveAll();
    isClose_ = false;
}

void HttpConn::Close() {
    response_.UnmapFile();
    if(isClose_ == false) {
        isClose_ = true;
        userCount--;
        close(fd_);
    }
}

int HttpConn::GetFd() const {
    return fd_;
}

int HttpConn::GetPort() const {
    return addr_.sin_port;
}

const char* HttpConn::GetIP() const {
    return inet_ntoa(addr_.sin_addr);
}

sockaddr_in HttpConn::GetAddr() const {
    return addr_;
}

ssize_t HttpConn::read(int* ErrorNo) {
    //读的接口，将文件描述符的内容读到读缓冲区
    ssize_t len = -1;
    do {
        len = readBuff_.ReadFd(fd_, ErrorNo);
        if(len <= 0) {
            break;
        }
    } while (isET);
    return len;
}

ssize_t HttpConn::write(int* ErrorNo) {
    //写的接口，将写缓冲区的内容写到文件描述符中
    ssize_t len = -1;
    do {
        len = writev(fd_, iov_, iovCount_);
        if(len <= 0) {
            *ErrorNo = errno;
            break;
        }
        if(iov_[0].iov_len + iov_[1].iov_len == 0) {
            break;
        }
        else if(static_cast<size_t>(len) > iov_[0].iov_len) {
            iov_[1].iov_base = (uint8_t*) iov_[1].iov_base + (len - iov_[0].iov_len);
            iov_[1].iov_len -= (len - iov_[0].iov_len);     //iov_len是可以写的字节长度
            if(iov_[0].iov_len) {
                writeBuff_.RetriveAll();
                iov_[0].iov_len = 0;
            }
        }
        else {
            iov_[0].iov_base = (uint8_t*) iov_[0].iov_base + len;
            iov_[0].iov_len -= len;
            writeBuff_.Retrieve(len);
        }

    } while (isET || ToWriteBytes() > 10240);
    return len;
}           

bool HttpConn::process() {
    //读写之前的初始化操作
    request_.Init();
    if(readBuff_.ReadableBytes() <= 0) {
        return false;
    }
    else if(request_.parse(readBuff_)) {
        //对从fd读到的数据进行解析
        response_.Init(srcDir, request_.path(), request_.IsKeepAlive(), 200);
        //初始化响应的时候 是用解析到请求的路径和iskeepalive进行初始化
    } else {
        response_.Init(srcDir, request_.path(), false, 400);
    }

    //响应头
    response_.MakeReponse(writeBuff_);
    iov_[0].iov_base = const_cast<char*>(writeBuff_.Peek());
    iov_[0].iov_len = writeBuff_.ReadableBytes();       //第二个bug,原来写成了WritableBytes(), 修改之后可以显示图片，没有样式
    //bug分析：写缓冲区可读的区域变大，write会写入多的字符，字符可能是未定义的字符引发错误
    iovCount_ = 1;

    //响应的页面
    if(response_.FileLen() > 0 && response_.File()) {
        iov_[1].iov_base = response_.File();
        iov_[1].iov_len = response_.FileLen();
        iovCount_ = 2;
    }
    return true;
}


