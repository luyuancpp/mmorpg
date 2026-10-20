#pragma once

#include <string>

#include <google/protobuf/message.h>
#include <google/protobuf/unknown_field_set.h>

// 未知字段跟着对象走(docs/design/mixed-version-persist-field-protection.md §4.2)。
//
// 新版本给持久消息加的字段,旧版本解析时落进消息本层的 UnknownFieldSet。直接以 PB 形式存放的组件(emplace 拷贝、
// CopyFrom 带回)天然保留它;由手写代码把 PB 拆成运行时结构、存盘时再逐字段拼回的地方,要用这一对函数把本层未知字段
// 存在运行时对象里,存盘时并回去。对象被删除或重建,未知字段跟着消失 —— 这是 protobuf 本身的语义,不会把旧对象的
// 新字段「复活」到同键的新对象上(设计 §4.1)。
//
// 存成序列化字节而不是 UnknownFieldSet:后者不可拷贝,而运行时结构要能放进 unordered_map、被拷贝。
namespace unknown_field_carry
{

// 取出消息本层(不含子消息)的未知字段;没有时返回空串,平时不占内存。
[[nodiscard]] inline std::string Capture(const google::protobuf::Message &message)
{
    const auto &unknown = message.GetReflection()->GetUnknownFields(message);
    std::string bytes;
    if (!unknown.empty())
    {
        (void)unknown.SerializeToString(&bytes);
    }
    return bytes;
}

// 先解析、后写入:调用方可以在动消息之前确认携带的字节完好。返回 false = 字节解析失败(只可能是内存被破坏)。
[[nodiscard]] inline bool Parse(const std::string &bytes, google::protobuf::UnknownFieldSet &out)
{
    out.Clear();
    return bytes.empty() || out.ParseFromString(bytes);
}

// 用携带的未知字段替换消息本层的未知字段。本层原有的先清掉:携带的这份就是本层的全部未知字段,
// 并入两份会让重复字段翻倍。
inline void Replace(const google::protobuf::UnknownFieldSet &carried, google::protobuf::Message &message)
{
    auto *unknown = message.GetReflection()->MutableUnknownFields(&message);
    unknown->Clear();
    unknown->MergeFrom(carried);
}

// Parse + Replace。返回 false 时消息不动,调用方必须拒绝这次写入。
[[nodiscard]] inline bool Restore(const std::string &bytes, google::protobuf::Message &message)
{
    google::protobuf::UnknownFieldSet parsed;
    if (!Parse(bytes, parsed))
    {
        return false;
    }
    Replace(parsed, message);
    return true;
}

// 消息本层是否带着本版本不认识的字段。旧代码对这类对象不做合堆、合并等会改变新字段语义的操作(设计 §4.4)。
[[nodiscard]] inline bool HasUnknown(const google::protobuf::Message &message)
{
    return !message.GetReflection()->GetUnknownFields(message).empty();
}

} // namespace unknown_field_carry
