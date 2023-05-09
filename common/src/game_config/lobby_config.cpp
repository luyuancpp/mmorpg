#include "lobby_config.h"

#include "google/protobuf/util/json_util.h"

#include "muduo/base/Logging.h"

#include "src/util/file2string.h"

using namespace common;

void LobbyConfig::Load(const std::string& filename)
{
    auto contents = File2String(filename);
    absl::string_view sv(contents.data(), contents.size());
    auto result = google::protobuf::util::JsonStringToMessage(sv, &config_info_);
    if (!result.ok()) {
        LOG_FATAL << result.message().data();
    }
}
