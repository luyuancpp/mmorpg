#pragma once

#include <optional>
#include <string_view>

// 严格提交屏障 C3 的模式(docs/design/mixed-version-persist-field-protection.md §5.2)。
//
// 部署键 PersistSchemaRegressionMode(BaseDeployConfig.persist_schema_regression_mode)只认两个字面量,
// 它们原样作为严格屏障 Lua 的 ARGV[10] 发出:
//   warn    照写父快照,返回 2(另计数),截断只告警不拦截;
//   enforce 拒写,返回 3,本次在线进度不落盘。
//
// 为什么在读配置时就归一化:proto3 的 string 缺省是空串,旧 ConfigMap 漏了这一键或写错时,
// 原值进 ARGV[10] 会让脚本返回 error_reply,而 OnSaved 对 ERROR 一律无限重试 —— 全服存不进盘。
// 所以缺键 / 空串按 warn(设计 §8.1:先 warn、演练通过后再切 enforce),其余非法值拒绝启动,
// 运行期只会见到这两个字面量。
namespace persist_schema_regression_mode
{

inline constexpr std::string_view kWarn = "warn";
inline constexpr std::string_view kEnforce = "enforce";

// 缺键与空串同义(proto3 无 presence),都归一化为 warn;warn / enforce 原样返回(区分大小写);
// 其余返回 nullopt,调用方必须拒绝启动,不能自行回落。
[[nodiscard]] inline std::optional<std::string_view> Normalize(std::string_view raw)
{
    if (raw.empty() || raw == kWarn)
    {
        return kWarn;
    }
    if (raw == kEnforce)
    {
        return kEnforce;
    }
    return std::nullopt;
}

} // namespace persist_schema_regression_mode
