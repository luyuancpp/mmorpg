#include "engine/core/utils/proto/proto_field_checker.h"
#include "proto/common/base/message.pb.h"  // proto-generated header
#include <gtest/gtest.h>
#include "gmock/gmock.h"

#include <google/protobuf/descriptor.h>
#include <google/protobuf/reflection.h>
#include <google/protobuf/message.h>

// 顶层 repeated 字段的阈值用例。原先借用 RouteMessageRequest.route_nodes,Route 一族已在批 7 从 proto 删除,
// 改用专为字段检查测试定义的 ProtoFieldCheckerTestSubPB.items;判据仍是「元素数 > threshold 才算超限」。
TEST(ProtoFieldCheckerTest, NoFieldExceedsThreshold) {
    ProtoFieldCheckerTestSubPB example;
    example.add_items("x");
    example.add_items("x");

    std::string output;
    bool result = ProtoFieldChecker::CheckFieldSizes(example, 5, output);

    EXPECT_FALSE(result);
    EXPECT_EQ(output, ""); // no field exceeds threshold
}

TEST(ProtoFieldCheckerTest, FieldExceedsThreshold) {
    ProtoFieldCheckerTestSubPB example;
    for (int i = 0; i < 10; ++i) {
        example.add_items("x");
    }

    std::string output;
    bool result = ProtoFieldChecker::CheckFieldSizes(example, 5, output);

    EXPECT_TRUE(result);
    EXPECT_NE(output.find("Field 'items' exceeds threshold"), std::string::npos);
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
    MyProtoMessage message; // replace with your actual Protobuf message type
    message.set_single_int32_field(10);    // positive int32
    message.set_single_int64_field(100);   // positive int64
    message.add_repeated_int32_field(5);   // positive int32
    message.add_repeated_int64_field(200); // positive int64

    bool result = ProtoFieldChecker::CheckForNegativeInts(message);

    EXPECT_FALSE(result); // should not detect negative values
}

TEST(ProtoFieldCheckerTest, CheckForNegativeInts_WithNegativeInt32) {
    MyProtoMessage message;
    message.set_single_int32_field(-10);  // negative int32
    message.set_single_int64_field(100); // positive int64

    bool result = ProtoFieldChecker::CheckForNegativeInts(message);

    EXPECT_TRUE(result); // should detect negative values
}

TEST(ProtoFieldCheckerTest, CheckForNegativeInts_WithNegativeInt64) {
    MyProtoMessage message;
    message.set_single_int32_field(10);    // positive int32
    message.set_single_int64_field(-100);  // negative int64
    message.add_repeated_int64_field(-200); // repeated negative int64

    bool result = ProtoFieldChecker::CheckForNegativeInts(message);

    EXPECT_TRUE(result); // should detect negative values
}

TEST(ProtoFieldCheckerTest, CheckForNegativeInts_WithRepeatedFields) {
    MyProtoMessage message;
    message.add_repeated_int32_field(5);    // positive int32
    message.add_repeated_int32_field(-15);  // negative int32
    message.add_repeated_int64_field(100);  // positive int64
    message.add_repeated_int64_field(-200); // negative int64

    bool result = ProtoFieldChecker::CheckForNegativeInts(message);

    EXPECT_TRUE(result); // should detect negative values
}

TEST(ProtoFieldCheckerTest, CheckForNegativeInts_WithNestedMessages) {
    MyProtoMessage message;
    MyNestedMessage* nested_message = message.mutable_nested_message(); // nested message
    nested_message->set_nested_int32_field(-15); // negative int32 in nested message
    nested_message->set_nested_int64_field(100); // positive int64 in nested message

    bool result = ProtoFieldChecker::CheckForNegativeInts(message);

    EXPECT_TRUE(result); // should detect negative values
}

TEST(ProtoFieldCheckerTest, CheckForNegativeInts_EmptyMessage) {
    MyProtoMessage message; // empty message

    bool result = ProtoFieldChecker::CheckForNegativeInts(message);

    EXPECT_FALSE(result); // empty message should not detect negative values
}
