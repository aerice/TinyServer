#progma once

#include <list>
#include <iostream>
#include <string>
#include <mutex>
#include <stdio.h>
#include <error.h>
#include <mysql/mysql.h>

static std::mutex mutex;

class connection_pool
{
public:
    connection_pool();
    ~connection_pool();

    MYSQL* GetConnection();  // 获取数据库连接
    bool ReleaseConnection(MYSQL* conn); // 释放连接
    void DestroyPool();

    //单例获取一个连接
    static connection_pool* GetInstance(std::string url, std::string User, std::string PassWord, 
        std::string DataName, int Port, unsigned int MaxConn);
    int GetFreeConnection() const; // 获取空闲连接数

private:
    connection_pool(const connection_pool&) = delete;
    void operator= (const connection_pool&) = delete;
private:
    unsigned int MaxConn;   // 最大连接数
    unsigned int CurConn;  // 当前连接数
    unsigned int FreeConn; // 当前空闲连接数
private:
    std::list<MYSQL*> connList; // 连接池
    connection_pool *conn;
    MYSQL* Con;
    connection_pool(std::string url, std::string User, std::string PassWord,
        std::string DataName, int Port, unsigned int MaxConn); // 构造
    static connection_pool* connPool;
private:
    std::string url;   // 主机地址
    std::string Port;   // 数据库端口
    std::string User;   // 登录数据库用户名
    std::string PassWord;  // 登录密码
    std::string DatabaseName;   // 使用数据库名
};

connection_pool* connection_pool::connPool = NULL;

connection_pool::connection_pool(std::string url, std::string User, std::string PassWord,
        std::string DataName, int Port, unsigned int MaxConn)
{
    this->url = url;
    this->User = User;
    this->PassWord = PassWord;
    this->DatabaseName = DataName;
    this->Port = Port;

    std::lock_guard<std::mutex> lock(mutex);
    for(int i = 0; i < MaxConn; ++i)
    {
        MYSQL* connectin = NULL;
        connectin = mysql_init(connectin);
        if (connectin == NULL) {
            std::cout << "Error: " << mysql_error(connectin);
            exit(1);
        }

        connectin = mysql_real_connect(connectin, url.c_str(), User.c_str(), PassWord.c_str(), DatabaseName.c_str(), Port, NULL, 0);
        if(connectin == NULL)
        {
            std::cout << "Error: " << mysql_error(connectin);
            exit(1);
        }
        connList.push_back(connectin);
        FreeConn++;
    }
    this->MaxConn = MaxConn;
    this->CurConn = 0;
}

// 获取实例，只有一个
connection_pool* connection_pool::GetInstance(std::string url, std::string User, std::string PassWord,
        std::string DataName, int Port, unsigned int MaxConn)
{
    // 判断是否为空，若为空则创建，不为空返回已存在的
    if(connPool == NULL) {
        std::lock_guard<std::mutex> lock(m_mutex)
        connPool = new connection_pool(url, User, PassWord, DataName, Port, MaxConn);
    }
    return connPool;
}

// 当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
MYSQL* connection_pool::GetConnection()
{
    MYSQL* connectin = NULL;
    std::lock_guard<std::mutex> lock(mutex);
    if(connList.size() > 0) {
        connectin = connList.front();
        connList.pop_front();

        FreeConn--;
        CurConn++;
    }
    return connectin;
}

// 释放当前使用连接
bool connection_pool::ReleaseConnection(MYSQL* connectin)
{
    std::lock_guard<std::mutex> lock(mutex);
    if(connectin != NULL) {
        connList.push_back(connectin);
        FreeConn++;
        CurConn--;
        return true;
    }
    return false;
}

// 销毁数据库连接池
void connection_pool::DestroyPool()
{
    std::lock_guard<std::mutex> lock(mutex);
    if(connList.size()) {
        //std::list<MYSQL*>::iterator it;
        for(auto& connectin : connList) {
            mysql_close(connectin);
        }
        CurConn = 0;
        FreeConn = 0;
        connList.clear();
    }
}

//
int connection_pool::GetFreeConnection() const
{
    return this->FreeConn;
}

// 析构
connection_pool::~connection_pool()
{
    DestroyPool();
}
