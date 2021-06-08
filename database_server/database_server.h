#ifndef DATABASE_SERVER_DATABASE_SERVER_H_
#define DATABASE_SERVER_DATABASE_SERVER_H_

#include "src/mysql_database/mysql_database.h"

#include "muduo/net/EventLoop.h"
#include "muduo/net/protorpc/RpcServer.h"

namespace database
{
    class DatabaseServer : muduo::noncopyable
    {
    public:
        DatabaseServer(muduo::net::EventLoop* loop,
            const muduo::net::InetAddress& listen_addr,
            const common::ConnectionParameters& db_cp)
            :server_(loop, listen_addr)
        {
            database_->Connect(db_cp);
        }

        void Start();

    private:
   
        DatabasePtr database_;
        muduo::net::RpcServer server_;
    };

}//namespace database

#endif//DATABASE_SERVER_DATABASE_SERVER_H_
