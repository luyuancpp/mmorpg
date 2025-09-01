#include "base/core/utils/proto/proto_field_checker.h"
#include "proto/common/message.pb.h"  // 你的 proto 生成的头文件
#include <gtest/gtest.h>
#include "gmock/gmock.h"

#include <google/protobuf/descriptor.h>
#include <google/protobuf/reflection.h>
#include <google/protobuf/message.h>

TEST(ProtoFieldCheckerTest, NoFieldExceedsThreshold) {
    RouteMessageRequest example;
    example.add_route_nodes();
    example.add_route_nodes();

    std::string output;
    bool result = ProtoFieldChecker::CheckFieldSizes(example, 5, output);

    EXPECT_FALSE(result);
    EXPECT_EQ(output, ""); // 没有字段超出阈值
}

TEST(ProtoFieldCheckerTest, FieldExceedsThreshold) {
    RouteMessageRequest example;
    for (int i = 0; i < 10; ++i) {
        example.add_route_nodes();
    }

    std::string output;
    bool result = ProtoFieldChecker::CheckFieldSizes(example, 5, output);

    EXPECT_TRUE(result);
    EXPECT_NE(output.find("Field 'route_nodes' exceeds threshold"), std::string::npos);
}

TEST(ProtoFieldCheckerTest, NestedFieldExceedsThreshold) {
    ProtoFieldCheckerTestPB example;
    auto* sub_message = example.mutable_sub_message();
    for (int i = 0; i < 10; ++i) {
        sub_message->add_items("item" + std::to_string(i));
    }

    std::string output;
    bool result = ProtoFieldChecker::CheckFieldSizes(example, 5, output);

    EXPECT_TRUE(result);
    EXPECT_NE(output.find("Field 'items' exceeds threshold"), std::string::npos);
}

TEST(ProtoFieldCheckerTest, CheckForNegativeInts_NoNegativeValues) {
    MyProtoMessage message; // 替换为你的实际 Protobuf 消息类型
    message.set_single_int32_field(10);    // 正数 int32
    message.set_single_int64_field(100);   // 正数 int64
    message.add_repeated_int32_field(5);   // 正数 int32
    message.add_repeated_int64_field(200); // 正数 int64

    bool result = ProtoFieldChecker::CheckForNegativeInts(message);

    EXPECT_FALSE(result); // 不应该检测到负数
}

TEST(ProtoFieldCheckerTest, CheckForNegativeInts_WithNegativeInt32) {
    MyProtoMessage message;
    message.set_single_int32_field(-10);  // 负数 int32
    message.set_single_int64_field(100); // 正数 int64

    bool result = ProtoFieldChecker::CheckForNegativeInts(message);

    EXPECT_TRUE(result); // 应该检测到负数
}

TEST(ProtoFieldCheckerTest, CheckForNegativeInts_WithNegativeInt64) {
    MyProtoMessage message;
    message.set_single_int32_field(10);    // 正数 int32
    message.set_single_int64_field(-100);  // 负数 int64
    message.add_repeated_int64_field(-200); // repeated 负数 int64

    bool result = ProtoFieldChecker::CheckForNegativeInts(message);

    EXPECT_TRUE(result); // 应该检测到负数
}

TEST(ProtoFieldCheckerTest, CheckForNegativeInts_WithRepeatedFields) {
    MyProtoMessage message;
    message.add_repeated_int32_field(5);    // 正数 int32
    message.add_repeated_int32_field(-15);  // 负数 int32
    message.add_repeated_int64_field(100);  // 正数 int64
    message.add_repeated_int64_field(-200); // 负数 int64

    bool result = ProtoFieldChecker::CheckForNegativeInts(message);

    EXPECT_TRUE(result); // 应该检测到负数
}

TEST(ProtoFieldCheckerTest, CheckForNegativeInts_WithNestedMessages) {
    MyProtoMessage message;
    MyNestedMessage* nested_message = message.mutable_nested_message(); // 嵌套消息
    nested_message->set_nested_int32_field(-15); // 嵌套消息中的负数 int32
    nested_message->set_nested_int64_field(100); // 嵌套消息中的正数 int64

    bool result = ProtoFieldChecker::CheckForNegativeInts(message);

    EXPECT_TRUE(result); // 应该检测到负数
}

TEST(ProtoFieldCheckerTest, CheckForNegativeInts_EmptyMessage) {
    MyProtoMessage message; // 空消息

    bool result = ProtoFieldChecker::CheckForNegativeInts(message);

    EXPECT_FALSE(result); // 空消息不应该检测到负数
}
