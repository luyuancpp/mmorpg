#include "deploy_json.h"

#include "google/protobuf/util/json_util.h"

#include "muduo/base/Logging.h"

#include "util/file2string.h"

void ZoneConfig::Load(const std::string& filename)
{
    auto contents = File2String(filename);
    absl::string_view sv(contents.data(), contents.size());
	auto result = google::protobuf::util::JsonStringToMessage(sv, &config_info_);
	if (!result.ok()) {
		LOG_FATAL << result.message().data();
	}
}

void DeployConfig::Load(const std::string& filename)
{
    auto contents = File2String(filename);
    absl::string_view sv(contents.data(), contents.size());
	auto result = google::protobuf::util::JsonStringToMessage(sv, &connetion_param_);
	if (!result.ok()) {
		LOG_FATAL << result.message().data();
	}
}

