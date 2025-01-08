#include "proto_field_checker.h"
#include <google/protobuf/descriptor.h>
#include <google/protobuf/reflection.h>
#include <sstream>

bool ProtoFieldChecker::CheckFieldSizes(const google::protobuf::Message& message, size_t threshold, std::string& output) {
    const google::protobuf::Descriptor* descriptor = message.GetDescriptor();
    const google::protobuf::Reflection* reflection = message.GetReflection();

    std::ostringstream os;
    bool exceeds_threshold = false;

    for (int i = 0; i < descriptor->field_count(); ++i) {
        const google::protobuf::FieldDescriptor* field = descriptor->field(i);

        if (field->is_repeated()) {
            int field_size = reflection->FieldSize(message, field);
            if (field_size > static_cast<int>(threshold)) {
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
    std::string output;
    return CheckFieldSizes(message, threshold, output);
}
