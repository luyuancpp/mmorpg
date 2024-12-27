#include "deploy_json.h"

#include "google/protobuf/util/json_util.h"
#include "muduo/base/Logging.h"
#include "util/file2string.h"

namespace {

    // 公共函数：加载 JSON 配置文件到 Protobuf 对象
    bool LoadJsonToProtobuf(const std::string& filename, google::protobuf::Message* protoMessage) {
        // 读取文件内容
        auto contents = File2String(filename);
        absl::string_view sv(contents.data(), contents.size());

        // 将 JSON 字符串解析为 Protobuf 消息
        auto result = google::protobuf::util::JsonStringToMessage(sv, protoMessage);
        if (!result.ok()) {
            LOG_FATAL << "Failed to load JSON from file " << filename << ": " << result.message().data();
            return false;
        }
        return true;
    }

}  // namespace

// ZoneConfig 类加载配置文件
void ZoneConfig::Load(const std::string& filename) {
    if (!LoadJsonToProtobuf(filename, &configInfo)) {
        LOG_FATAL << "ZoneConfig loading failed!";
    }
}

// DeployConfig 类加载配置文件
void DeployConfig::Load(const std::string& filename) {
    if (!LoadJsonToProtobuf(filename, &connectionParam)) {
        LOG_FATAL << "DeployConfig loading failed!";
    }
}
