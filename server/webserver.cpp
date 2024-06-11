#include "webserver.h"

using namespace std;

WebServer::WebServer(
    int port, int trigeMode, int timeoutMS, bool OptLinger, 
        int sqlPort, const char* sqlUser, const char* sqlPwd,
        const char* dbName, int connPoolNum, int threadNum)
{
    port_ = port;
    openLinger_ = OptLinger;
    timeoutMS_ = timeoutMS;
    isClose_ = false;
    threadpool_ = std::make_unique<ThreadPool>(threadNum);
    epoller_ = std::make_unique<Epoller>();

    srcDir_ = getcwd(nullptr, 256);         //获取当前工作目录的路径
    assert(srcDir_);
    strncat(srcDir_, "/resources/", 16);    //将src字符串中的n个字符追加到secDir_的末尾，并在追加字符串后添加一个空字符'\0'作为字符串结束标志
    //但字符串长度小于n时，只会追加src字符串中的内容
    HttpConn::userCount = 0;
    HttpConn::srcDir = srcDir_;
    SqlConnPool::Instance()->Init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);
    InitEventMode_(trigeMode);
    if(!InitSocket_()) {
        isClose_ = true;
    }
}

WebServer::~WebServer() {
    close(listenFd_);
    isClose_ = true;
    free(srcDir_);
    SqlConnPool::Instance()->ClosePool();
}

void WebServer::start() {
    int timeMS = -1;
    if(!isClose_) {
        printf("Server start");
    }
    while(!isClose_) {
        int eventCount = epoller_->Wait(timeMS);
        for(int i = 0; i < eventCount; i++) {
            int fd = epoller_->GetEventFd(i);
            uint32_t events = epoller_->GetEvent(i);
            if(fd == listenFd_) {
                DealListen_();
            }
            else if(events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                //?参数的意思   读挂起 | 挂起 | 错误
                //对端关闭写，当前端可以继续读 | 文件描述符被关闭 | 文件描述符上发生了错误
                assert(users_.count(fd) > 0);
                CloseConn_(&users_[fd]);
            }
            else if(events & EPOLLIN) {
                assert(users_.count(fd) > 0);
                DealRead_(&users_[fd]);
            }
            else if(events & EPOLLOUT) {
                assert(users_.count(fd) > 0);
                DealWrite_(&users_[fd]);
            }
            else {
                printf("Unexcepcted event");
            }
        }
    }
}

bool WebServer::InitSocket_() {
    int ret;
    struct sockaddr_in addr;
    if(port_ > 65535 || port_ < 1024) {
        return false;
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);           //bug，原来没加htons，addr格式错误，不能识别端口，不能正常连接
    struct linger optLinger = {0};
    if(openLinger_) {
        //优雅关闭 ： 控制在close / shutdown被调用之后，套接字是否立即关闭或者等待剩余的数据发送完毕
        //参数是什么意思？
        optLinger.l_onoff = 1;      //选项启用
        optLinger.l_linger = 1;     //延迟时间（秒）
    }

    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if(listenFd_ < 0) {
        return false;
    }

    ret = setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if(ret < 0) {
        close(listenFd_);
        return false;
    }
    //端口复用 ： 允许多个套接字绑定到同一个端口的机制
    //?参数含义: 允许重新绑定处于TIME_WAIT状态的端口，在服务器重启时非常有用
    int optval = 1;
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    if(ret < 0) {
        close(listenFd_);
        return false;
    }

    ret = bind(listenFd_, (struct sockaddr*)&addr, sizeof(addr));
    if(ret < 0) {
        close(listenFd_);
        return false;
    }

    ret = listen(listenFd_, 6);
    if(ret < 0) {
        close(listenFd_);
        return false;
    }

    SetFdNonblock(listenFd_);
    ret = epoller_->AddFd(listenFd_, listenEvent_ | EPOLLIN);
    if(ret == 0) {
        close(listenFd_);
        return false;
    }

    return true;
}

void WebServer::InitEventMode_(int trigeMode) {
    //？连接事件和监听事件的区别
    //listenEvent_用于对用户建立连接时使用
    //connEvent_用于处理事件时使用
    listenEvent_ = EPOLLRDHUP;      //这选项是什么意思？半挂起状态
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP;
    //EPOLLONESHOT只触发一次事件通知，当文件描述符上事件被触发且处理之后，会自动从监视列表移除，除非重新注册该文件描述符！！！
    //在多线程环境中可以避免同一个事件被多个线程处理,确保线程安全
    switch(trigeMode) {
        case 0:
            break;
        case 1:
            connEvent_ |= EPOLLET;
            break;
        case 2:
            listenEvent_ |= EPOLLET;
            break;
        case 3:
            connEvent_ |= EPOLLET;
            listenEvent_ |= EPOLLET;
            break;
        default:
            connEvent_ |= EPOLLET;
            listenEvent_ |= EPOLLET;
            break;
    }
    HttpConn::isET = (connEvent_ & EPOLLET);
}

void WebServer::AddClient_(int fd, sockaddr_in addr) {
    assert(fd > 0);
    users_[fd].Init(fd, addr);
    SetFdNonblock(fd);  //这个顺序对吗？先添加，在设置为非阻塞
    epoller_->AddFd(fd, EPOLLIN | connEvent_);
}

void WebServer::DealListen_() {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    do {
        int fd = accept(listenFd_, (struct sockaddr *)&addr, &len);
        if(fd <= 0) {
            return;
        } else if(HttpConn::userCount >= MAX_FD) {
            //当超出最大连接数量时，向请求的客户端fd发送错误信息
            SendError_(fd, "server busy");
            printf("client is full");
            return;
        }
        AddClient_(fd, addr);       //添加用户
    } while(listenEvent_ & EPOLLET);
}

void WebServer::DealWrite_(HttpConn* client) {
    assert(client);
    //std::bind什么意思？ 绑定函数模版和参数
    threadpool_->AddTask(std::bind(&WebServer::OnWrite_, this, client));    
}

void WebServer::DealRead_(HttpConn* client) {
    assert(client);
    threadpool_->AddTask(std::bind(&WebServer::OnRead_, this, client));
}

void WebServer::SendError_(int fd, const char* info) {
    assert(fd > 0);
    int ret = send(fd, info, sizeof(info), 0);  //flag = 0代表什么意思：默认操作，没有特殊含义
    if(ret < 0) {
        printf("send error to clientfd: %d", fd);
    }
    close(fd);
}   
   
// void WebServer::ExtentTime_(HttpConn* client) {

// }   

void WebServer::CloseConn_(HttpConn* client) {
    assert(client);
    epoller_->DelFd(client->GetFd());
    client->Close();
}              

void WebServer::OnRead_(HttpConn* client) {
    assert(client);
    int ret = -1;
    int readErrno = 0;
    ret = client->read(&readErrno);
    if(ret <= 0 && readErrno != EAGAIN) {       //第一个大bug，如果是||，会提前关闭连接，没有响应，修改之后有相应，没有图片和样式
        CloseConn_(client);
        return;
    }
    OnProcess(client);
}

void WebServer::OnWrite_(HttpConn* client) {
    assert(client);
    int ret = -1;
    int writeErrno = 0;
    ret = client->write(&writeErrno);
    if(client->ToWriteBytes() == 0) {
        //传输完成, 对应的是http响应传输完成
        if(client->IsKeepAlive()) {
            OnProcess(client);
            return;
        }
    } else if(ret < 0) {
        if(writeErrno == EAGAIN) {
            //继续传输
            epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
            return;
        }
    }
    CloseConn_(client);
}

void WebServer::OnProcess(HttpConn* client) {
    if(client->process()) {
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);    //准备好进行写操作
    } else {
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN);     //准备好进行读操作
    }
}

int WebServer::SetFdNonblock(int fd) {
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);      //?第三个参数什么意思
    //fcntl返回的就是状态标志，第三个参数flag，fcntl(fd, F_GETFD, 0)是返回文件描述符当前的状态
}