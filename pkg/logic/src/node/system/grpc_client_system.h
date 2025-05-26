#pragma once
#include <string>
#include <vector>
#include <google/protobuf/repeated_field.h>

namespace GrpcClientSystem {
	void InitEtcdStubs(const ::google::protobuf::RepeatedPtrField<std::string>& etcdHosts);
}
