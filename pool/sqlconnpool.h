#ifndef SQLCONNPOOL_H
#define SQLCONNPOOL_H

#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <mutex>
#include <semaphore.h>
#include <thread>
#include <assert.h>

class SqlConnPool {
public :
    static SqlConnPool *Instance();     //获取唯一实例的全局访问点
    
    //初始化连接池
    void Init(const char* host, int port, const char* user, const char* pwd, const char* dbName, int connSize);
    void ClosePool();               //关闭连接池，释放连接
    MYSQL* GetConn();               //从连接池中获取一个MYSQL*的连接
    void FreeConn(MYSQL* sql);     //释放一个MYSQL*连接回连接池
    int GetFreeConnCount();         //获取空闲的连接数量

private :
    SqlConnPool();
    ~SqlConnPool();
    SqlConnPool(const SqlConnPool&) = delete;
    SqlConnPool& operator=(const SqlConnPool&) = delete;
    // static SqlConnPool* instance_;    //唯一实例 显式的写法

    int Max_Conn_;   //最大连接数
    int useCount_;  //使用的连接数
    int freeCount_; //空闲的连接数

    std::queue<MYSQL*> connQueue_;      //MYSQL*是对数据库的一个持久连接，不显式释放的话会一直连接，反复的连接会消耗系统资源，所以用连接池的设计
    std::mutex mtx_;    //互斥锁，控制对连接队列的互斥访问
    sem_t semId_;       //同步互斥信号量，限制同时可以获取的连接数量
    //设计思路：sem_t用Max_Conn_的值进行初始化，控制最大连接数，在建立连接时sem_wait,释放连接时sem_post
    //mutex对队列的访问进行上锁，在队列初始化完之后，运行在多线程环境中时，访问队列的操作都要上锁
};

#endif //SQLCONNPOOL_H