#include "epoller.h"


/*
    对epoll函数的封装
    epoll_create(int size) //size > 0
    epoll_wait(int epollfd, struct epoll_event events, int maxEvents, int timeoutMs)   //events是返回的事件集合
    epoll_ctl(int epollfd, int op, int fd, struct epoll_event event)    //op是对epollfd要进行的操作，fd和event是操作的目标，
*/

Epoller::Epoller(int maxEvent) : epollfd_(epoll_create(512)), events_(maxEvent) {
    assert(events_.size() >= 0 && epollfd_ >= 0);
}

Epoller::~Epoller() {
    close(epollfd_);
}

bool Epoller::AddFd(int fd, uint32_t events) {
    if(fd < 0)
        return false;
    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;
    return 0 == epoll_ctl(epollfd_, EPOLL_CTL_ADD, fd, &ev);
}

bool Epoller::ModFd(int fd, uint32_t events) {
    if(fd < 0)
        return false;
    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;
    return 0 == epoll_ctl(epollfd_, EPOLL_CTL_MOD, fd, &ev);        
}

bool Epoller::DelFd(int fd) {
    if(fd < 0)
        return false;
    epoll_event ev = {0};
    return 0 == epoll_ctl(epollfd_, EPOLL_CTL_DEL, fd, &ev);
}

int Epoller::Wait(int TimeoutMs) {
    return epoll_wait(epollfd_, &events_[0], static_cast<int>(events_.size()), TimeoutMs);
}

int Epoller::GetEventFd(size_t i) const {
    assert(i < events_.size() && i >= 0);
    return events_[i].data.fd;
}

uint32_t Epoller::GetEvent(size_t i) const{
    assert(i < events_.size() && i >= 0);
    return events_[i].events;
}
