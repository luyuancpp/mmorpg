#include "util/proto_field_checker.h"
#include "proto/common/common_message.pb.h"  // 你的 proto 生成的头文件
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
