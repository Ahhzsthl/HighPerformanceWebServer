#ifndef SQLCONNRAII_H
#define SQLCONNRAII_H
#include "sqlconnpool.h"

//资源获取即初始化的思想，resource acquisition is initialization 
class SqlConnRAII {
public : 
    SqlConnRAII(MYSQL** sql, SqlConnPool* connpool) {
        assert(connpool);
        *sql = connpool->GetConn();
        sql_ = *sql;
        connpool_ = connpool;
    }
    //用二维指针或者引用的方式实现
    //因为是要修改*sql指针的值，如果传递的是一维指针的话，函数中修改的是经过拷贝赋值函数的副本，原来的sql还是nullptr
    //二维指针是对*sql的地址的值做修改，改的就是*sql
    SqlConnRAII(MYSQL* &sql, SqlConnPool *connpool) {
        assert(connpool);
        sql = connpool->GetConn();
        sql_ = sql;
        connpool_ = connpool;
    }

    ~SqlConnRAII() {
        if(sql_) {
            connpool_->FreeConn(sql_);
        }
    }

private : 
    MYSQL* sql_;
    SqlConnPool* connpool_;
};


#endif