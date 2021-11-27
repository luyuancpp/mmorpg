#include "region_config.h"

#include "google/protobuf/util/json_util.h"

#include "src/file2string/file2string.h"

namespace common
{
void RegionConfig::Load(const std::string& filename)
{
    auto contents = File2String(filename);
    google::protobuf::StringPiece sp(contents.data(), contents.size());
    google::protobuf::util::JsonStringToMessage(sp, &config_info_);
}
}//namespace region