#include "region_config.h"

#include "google/protobuf/util/json_util.h"

#include "src/util/file2string.h"

using namespace common;

void RegionConfig::Load(const std::string& filename)
{
    auto contents = File2String(filename);
    google::protobuf::StringPiece sp(contents.data(), contents.size());
    google::protobuf::util::JsonStringToMessage(sp, &config_info_);
}
