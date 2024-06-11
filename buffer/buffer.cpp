#include "buffer.h"

Buffer::Buffer(int BufferSize) : buffer_(BufferSize), readPos_(0), writePos_(0) {

}

size_t Buffer::WritableBytes() const {
    return buffer_.size() - writePos_;
}

size_t Buffer::ReadableBytes() const {
    return writePos_ - readPos_;
}

size_t Buffer::PrependableBytes() const {
    return readPos_;
}

const char* Buffer::Peek() const {
    return BeginPtr_() + readPos_;
}

const char* Buffer::BeginWriteConst() const {
    return BeginPtr_() + writePos_;
}

char* Buffer::BeginWrite() {
    return BeginPtr_() + writePos_;
}

void Buffer::EnsureWritable(size_t len) {
    if(WritableBytes() < len) {             //第三个bug，写反了
        MakeSpace_(len);
    }
    assert(WritableBytes() >= len);
}

void Buffer::HasWritten(size_t len) {
    writePos_ += len;
}

void Buffer::Retrieve(size_t len) {
    assert(len <= ReadableBytes());
    readPos_ += len;
}

void Buffer::RetrieveUntil(const char* end) {
    assert(Peek() <= end);
    Retrieve(end - Peek());
}

void Buffer::RetriveAll() {
    bzero(&buffer_[0], buffer_.size());
    readPos_ = 0;
    writePos_ = 0;
}

std::string Buffer::RetriveAllToStr() {
    std::string str(Peek(), ReadableBytes());       //将起始read指针位置长度为ReadableBytes的字符转为字符串
    RetriveAll();
    return str;
}

void Buffer::Append(const std::string& str) {
    Append(str.data(), str.length());
}

void Buffer::Append(const char* str, size_t len) {
    assert(str);
    EnsureWritable(len);
    std::copy(str, str + len, BeginWrite());
    HasWritten(len);
}

void Buffer::Append(const void* data, size_t len) {
    assert(data);
    Append(static_cast<const char*>(data), len);    //static_cast有什么好处？
    //编译时，编译器会检查转换是否合法，所以转换更安全，不会允许不安全的转换发生
}

void Buffer::Append(const Buffer& buff) {
    Append(buff.Peek(), buff.WritableBytes());
}

char* Buffer::BeginPtr_() {
    return &*buffer_.begin();
}

const char* Buffer::BeginPtr_() const {
    return &*buffer_.begin();           //？为什么&* ： buffer.begin()返回第一个元素，*返回第一个元素的引用，&*返回第一个元素的指针
    // return buffer_.data();           等同于上面的写法
}

void Buffer::MakeSpace_(size_t len) {
    if(WritableBytes() + PrependableBytes() < len) {
        buffer_.resize(writePos_ + len + 1);
    }
    else {
        size_t readable = ReadableBytes();
        std::copy(BeginPtr_() + readPos_, BeginPtr_() + writePos_, BeginPtr_());
        readPos_ = 0;
        writePos_ = readPos_ + readable;        //writePos_ = readable
        assert(readable == ReadableBytes());    //确保数据完整性
    }
}

ssize_t Buffer::ReadFd(int fd, int* Error) {
    char buff[65535];
    struct iovec iov[2];
    const size_t writable = WritableBytes();
    //分散读，保证一次性读完
    iov[0].iov_base = BeginPtr_() + writePos_;
    iov[0].iov_len = writable;
    iov[1].iov_base = buff;
    iov[1].iov_len = sizeof(buff);

    const ssize_t len = readv(fd, iov, 2);
    if(len < 0) {
        *Error = errno;
    } else if (static_cast<size_t>(len) <= writable) {
        writePos_ += len;
    } else {
        writePos_ = buffer_.size();        //先更新写指针的位置，说明写缓冲区已经写满了，此时需要判断是否需要扩大缓冲区
        Append(buff, len - writable);
    }
    return len;
}

ssize_t Buffer::WriteFd(int fd, int* Error) {
    ssize_t readSize = ReadableBytes();
    ssize_t len = write(fd, Peek(), readSize);
    if(len < 0) {
        *Error = errno;
        return len;
    }
    readPos_ += len;
    return len;
}