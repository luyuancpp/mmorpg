#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>

void copyCommonFields(
	const google::protobuf::Message& src,
	google::protobuf::Message& dst)
{
	const auto* desc = src.GetDescriptor();
	const auto* refl_src = src.GetReflection();
	const auto* refl_dst = dst.GetReflection();

	for (int i = 0; i < desc->field_count(); ++i)
	{
		const auto* field = desc->field(i);

		if (!refl_src->HasField(src, field)) continue;

		switch (field->cpp_type())
		{
		case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
			refl_dst->SetInt32(&dst, field, refl_src->GetInt32(src, field));
			break;
		case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
			refl_dst->SetInt64(&dst, field, refl_src->GetInt64(src, field));
			break;
		case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
			refl_dst->SetUInt32(&dst, field, refl_src->GetUInt32(src, field));
			break;
		case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
			refl_dst->SetUInt64(&dst, field, refl_src->GetUInt64(src, field));
			break;
		case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
			refl_dst->SetFloat(&dst, field, refl_src->GetFloat(src, field));
			break;
		case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
			refl_dst->SetDouble(&dst, field, refl_src->GetDouble(src, field));
			break;
		case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
			refl_dst->SetBool(&dst, field, refl_src->GetBool(src, field));
			break;
		case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
			refl_dst->SetString(&dst, field, refl_src->GetString(src, field));
			break;
		case google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
			refl_dst->SetEnumValue(&dst, field, refl_src->GetEnumValue(src, field));
			break;
		case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
			refl_dst->MutableMessage(&dst, field)->CopyFrom(
				refl_src->GetMessage(src, field));
			break;
		}
	}
}
