#include <vector>
#include <sys/epoll.h>
#include <cassert>  //assert
#include <unistd.h> //close

/*
    对epoll函数的封装
    epoll_create(int size) //size > 0
    epoll_wait(int epollfd, struct epoll_event events, int maxEvents, int timeoutMs)   //events是返回的事件集合
    epoll_ctl(int epollfd, int op, int fd, struct epoll_event event)    //op是对epollfd要进行的操作，fd和event是操作的目标，
*/
class Epoller {
public :
    explicit Epoller(int maxEvent = 1024);
    ~Epoller();
    bool AddFd(int fd, uint32_t events);
    bool ModFd(int fd, uint32_t events);
    bool DelFd(int fd);
    int Wait(int timeoutMs = -1);
    int GetEventFd(size_t i) const;
    uint32_t GetEvent(size_t i) const;

private :
    int epollfd_;
    std::vector<struct epoll_event> events_;
    
};