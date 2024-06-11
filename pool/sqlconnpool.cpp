#include "sqlconnpool.h"

// SqlConnPool* SqlConnPool::instance_ = nullptr;

SqlConnPool::SqlConnPool() {
    useCount_ = 0;
    freeCount_ = 0;
}

SqlConnPool::~SqlConnPool() {
    ClosePool();
}

SqlConnPool* SqlConnPool::Instance() {
    // if(instance_ == nullptr) {
    //     instance_ = new SqlConnPool();   //显式申请内存
    // }
    static SqlConnPool instance_;   //静态局部变量，第一次调用Instance方法时创建，程序结束时自动销毁
    return &instance_;
}

void SqlConnPool::Init(const char* host, int port, 
    const char* user, const char* pwd, 
    const char* dbName, int connSize = 10) {
    assert(connSize > 0);
    for(int i = 0; i < connSize; i++) {
        MYSQL* sql = nullptr;
        sql = mysql_init(sql);
        if(!sql) {
            assert(sql);
        }
        sql = mysql_real_connect(sql, host, user, pwd, dbName, port, nullptr, 0);
        if(!sql) {
            assert(sql);
        }
        connQueue_.push(sql);
    }
    Max_Conn_ = connSize;
    sem_init(&semId_, 0, Max_Conn_);    
    //0表示用于线程之间的同步，进程之间的同步需要设置为非0
    //第三个参数代表信号量的初始值
}

MYSQL* SqlConnPool::GetConn() {
    MYSQL* sql = nullptr;
    if(connQueue_.empty()) {
        return nullptr;
    }
    sem_wait(&semId_);      //获取一个信号量
    {
        std::lock_guard<std::mutex> locker(mtx_);
        sql = connQueue_.front();
        connQueue_.pop();
    }
    return sql;
}

void SqlConnPool::FreeConn(MYSQL* sql) {
    assert(sql);
    std::lock_guard<std::mutex> locker(mtx_);
    connQueue_.push(sql);
    sem_post(&semId_);  //释放一个信号量
}

int SqlConnPool::GetFreeConnCount() {
    std::lock_guard<std::mutex> locker(mtx_);
    return connQueue_.size();
}

void SqlConnPool::ClosePool() {
    // if(instance_ != nullptr) {
    //     //实例是通过new申请的，需要用delete主动销毁
    //     delete instance_;
    //     instance_ = nullptr;
    // }
    std::lock_guard<std::mutex> locker(mtx_);
    while(!connQueue_.empty()) {
        auto item = connQueue_.front();
        connQueue_.pop();
        mysql_close(item);
    }
    mysql_library_end();    //和mysql_library_init成对使用，用于多线程环境中初始化资源
    //TODO : 但是mysql_library_init好像没有调用
}

