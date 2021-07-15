#ifndef DATABASE_SERVER_SRC_CONFIG_DATABASE_CONFIG_H_
#define DATABASE_SERVER_SRC_CONFIG_DATABASE_CONFIG_H_

#include "common.pb.h"

namespace database
{
    class DatabaseConfig
    {
    public:
        const ServerInfo& deploy_server() const { return deploy_server_; }

        static DatabaseConfig& GetSingleton() {
            static DatabaseConfig singleton;
            return singleton;
        }

        void Load(const std::string& filename);
    private:
        ServerInfo deploy_server_;
    };
}//namespace database

#endif//DATABASE_SERVER_SRC_CONFIG_DATABASE_CONFIG_H_
