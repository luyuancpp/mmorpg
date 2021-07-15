#include "database_config.h"

#include "google/protobuf/util/json_util.h"

#include "src/file2string/file2string.h"

namespace database
{
void DatabaseConfig::Load(const std::string& filename)
{
    auto contents = common::File2String(filename);
    google::protobuf::StringPiece sp(contents.data(), contents.size());
    google::protobuf::util::JsonStringToMessage(sp, &deploy_server_);
}
}//namespace database
