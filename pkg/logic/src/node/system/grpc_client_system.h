#pragma once
#include <string>
#include <vector>
#include <google/protobuf/repeated_field.h>

namespace GrpcClientManager {
	void InitEtcdStubs(const ::google::protobuf::RepeatedPtrField<std::string>& etcdHosts);
	void InitDeployService(const std::string& deployAddress);
}
