#ifndef BUFFER_H
#define BUFFER_H

#include <vector>
#include <atomic>
#include <iostream>
#include <unistd.h>     //size_t 
#include <assert.h>
#include <string.h>     //bzero 
#include <sys/uio.h>    //iovec readv writev

class Buffer {
public : 
    Buffer(int BufferSize = 1024);
    ~Buffer() = default;

    size_t WritableBytes() const;           //可写的字节数
    size_t ReadableBytes() const;           //可读的字节数
    size_t PrependableBytes() const;        //预备留空的字节数

    const char* Peek() const;               //可以读入的指针起始地址
    const char* BeginWriteConst() const;    //可以写入的指针起始地址
    char* BeginWrite();

    void EnsureWritable(size_t len);        //确定len长度的字节是否可以写入缓冲区
    void HasWritten(size_t len);            //更新缓冲区写指针位置，

    //提取主要就是更新读缓冲区的位置，更新剩余的可读字节数
    void Retrieve(size_t len);              //提取读缓冲区内len字节的数据
    void RetrieveUntil(const char* end);    //提取读缓冲区内end前的数据
    void RetriveAll();                      //提取读缓冲区内全部数据
    std::string RetriveAllToStr();          //将读缓冲区内所有数据转换为字符串

    void Append(const std::string& str);
    void Append(const char* str, size_t len);   //向写缓冲区增加数据，会有增大缓冲区的操作
    void Append(const void* data, size_t len);  
    void Append(const Buffer& buff);

    ssize_t ReadFd(int fd, int* Error);         //将fd数据读入缓冲区
    ssize_t WriteFd(int fd, int* Error);        //将缓冲区数据写到fd中

private : 
    char* BeginPtr_();
    const char* BeginPtr_() const;
    void MakeSpace_(size_t len);              //将缓冲区增大len长度,如果长度够的话就将缓冲区前移

    std::vector<char> buffer_;
    std::atomic<std::size_t> readPos_;        //读指针距离开始的字节数
    std::atomic<std::size_t> writePos_;       //写指针距离开始的字节数
};

#endif