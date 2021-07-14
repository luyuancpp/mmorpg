#include "deploy_config.h"

#include "google/protobuf/util/json_util.h"

#include "src/file2string/file2string.h"

namespace deploy_server
{

void DeployConfig::Load(const std::string& filename)
{
    auto contents = common::File2String(filename);
    google::protobuf::StringPiece sp(contents.data(), contents.size());
    google::protobuf::util::JsonStringToMessage(sp, &connetion_param_);
}

}//namespace deploy_server

