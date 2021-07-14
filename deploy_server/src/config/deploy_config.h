#ifndef SRC_DEPLOY_SERVER_SRC_CONFIG_DEPLOY_CONFIG_H_
#define SRC_DEPLOY_SERVER_SRC_CONFIG_DEPLOY_CONFIG_H_

#include "common.pb.h"

namespace deploy_server
{
    class DeployConfig
    {
    public:
        const ConnetionParam& connetion_param() { return connetion_param_.data(0); }

        void Load(const std::string& filename);
        
    private:
        ConnetionParamJsonFormat connetion_param_;
    };
}//namespace deploy_server


#endif//SRC_DEPLOY_SERVER_SRC_CONFIG_DEPLOY_CONFIG_H_
