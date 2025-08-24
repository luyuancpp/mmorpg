#include "proto_field_checker.h"
#include <google/protobuf/descriptor.h>
#include <google/protobuf/reflection.h>
#include <sstream>
#include <muduo/base/Logging.h>

bool ProtoFieldChecker::CheckFieldSizes(const google::protobuf::Message& message, size_t threshold, std::string& output) {
    const google::protobuf::Descriptor* descriptor = message.GetDescriptor();
    const google::protobuf::Reflection* reflection = message.GetReflection();

    std::ostringstream os;
    bool exceeds_threshold = false;

    for (int i = 0; i < descriptor->field_count(); ++i) {
        const google::protobuf::FieldDescriptor* field = descriptor->field(i);

        if (field->is_repeated()) {
            if (const int32_t field_size = reflection->FieldSize(message, field);
                field_size > static_cast<int32_t>(threshold)) {
                exceeds_threshold = true;
                os << "Field '" << field->name() << "' exceeds threshold with size: " << field_size << "\n";
            }
        }
        else if (field->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE) {
            const google::protobuf::Message& nested_message = reflection->GetMessage(message, field);
            exceeds_threshold |= CheckFieldSizes(nested_message, threshold, output); // 递归检查嵌套字段
        }
    }

    output += os.str();
    return exceeds_threshold;
}

bool ProtoFieldChecker::CheckFieldSizes(const google::protobuf::Message& message, size_t threshold) {
    std::string error_details; // 改名为 error_details，表示存储错误细节的字符串
    const bool has_exceeding_fields = CheckFieldSizes(message, threshold, error_details); // result 改为 has_exceeding_fields，清楚表明其含义

    if (!has_exceeding_fields) {
        LOG_ERROR << error_details; // 更清楚地表明是错误细节被输出
    }

    return has_exceeding_fields;
}



bool ProtoFieldChecker::CheckForNegativeInts(const google::protobuf::Message& message, std::string& output) {
    const google::protobuf::Descriptor* descriptor = message.GetDescriptor();
    const google::protobuf::Reflection* reflection = message.GetReflection();

    std::ostringstream os;
    bool has_negative = false;

    for (int i = 0; i < descriptor->field_count(); ++i) {
        const google::protobuf::FieldDescriptor* field = descriptor->field(i);

        if (field->is_repeated()) {
            // 检查 repeated int 类型字段是否有负数
            if (field->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_INT32 ||
                field->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_INT64) {
                const int32_t field_size = reflection->FieldSize(message, field);
                for (int32_t j = 0; j < field_size; ++j) {
                    const int64_t value = (field->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_INT32)
                        ? reflection->GetRepeatedInt32(message, field, j)
                        : reflection->GetRepeatedInt64(message, field, j);

                    if (value < 0) {
                        os << "Field '" << field->name() << "' contains negative value: " << value << "\n";
                        has_negative = true;
                    }
                }
            }
        }
        else if (field->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE) {
            // 递归检查嵌套消息
            const google::protobuf::Message& nested_message = reflection->GetMessage(message, field);
            has_negative |= CheckForNegativeInts(nested_message, output);
        }
        else if (field->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_INT32) {
            if (const int32_t value = reflection->GetInt32(message, field); value < 0) {
                os << "Field '" << field->name() << "' contains negative value: " << value << "\n";
                has_negative = true;
            }
        }
        else if (field->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_INT64) {
            if (const int64_t value = reflection->GetInt64(message, field); value < 0) {
                os << "Field '" << field->name() << "' contains negative value: " << value << "\n";
                has_negative = true;
            }
        }
    }

    output += os.str();
    return has_negative;
}

bool ProtoFieldChecker::CheckForNegativeInts(const google::protobuf::Message& message) {
    std::string negative_value_details; // 原 output 改为 negative_value_details，更明确表示存储负值的细节信息
    const bool has_negative_values = CheckForNegativeInts(message, negative_value_details); // 原 result 改为 has_negative_values，更清晰地表示其作用

    if (has_negative_values) {
        LOG_ERROR << negative_value_details; // 输出详细的负值信息
    }

    return has_negative_values;
}
