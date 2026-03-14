#pragma once

class NodeAllocator {
public:
	static void AcquireNode();
	static void AcquireNodePort();
	static void ReRegisterExistingNode();
};