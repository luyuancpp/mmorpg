#pragma once

#include <cstdint>

//服务器重启以后失效的
template <class T, size_t kNodeBit>
class TransientNodeCompositeIdGenerator
{
public:
	static size_t node_bit() { return kNodeBit; }

	void set_node_id(T node_id)
	{
		node_id_ = node_id << kNodeBit;
	}

	T node_id(T guid)
	{
		return guid >> kNodeBit;
	}

	T Generate()
	{
		return node_id_ | ++seq_;
	}

	T LastId()
	{
		return  node_id_ | seq_;
	}
private:
	T node_id_{ 0 };
	T seq_{ 0 };
};

using TransientNode12BitCompositeIdGenerator  = TransientNodeCompositeIdGenerator<uint32_t, 12>;
