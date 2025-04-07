#pragma once
#include "proto/logic/constants/node.pb.h"

class NodeSystem
{
public:
	static eNodeType GetServiceTypeFromPrefix(const std::string& prefix);
};

