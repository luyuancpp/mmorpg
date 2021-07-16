#include "game_config.h"

#include "google/protobuf/util/json_util.h"

#include "src/file2string/file2string.h"

namespace common
{
void GameJson::Load(const std::string& filename)
{
    auto contents = File2String(filename);
    google::protobuf::StringPiece sp(contents.data(), contents.size());
    google::protobuf::util::JsonStringToMessage(sp, &deploy_server_);
}

void DeployConfig::Load(const std::string& filename)
{
    auto contents = common::File2String(filename);
    google::protobuf::StringPiece sp(contents.data(), contents.size());
    google::protobuf::util::JsonStringToMessage(sp, &connetion_param_);
}
}//namespace database
