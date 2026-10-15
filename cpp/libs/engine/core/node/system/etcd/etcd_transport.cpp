#include "node/system/etcd/etcd_transport.h"

#include <utility>

#include "node/system/etcd/grpc_etcd_transport.h"

namespace etcd_transport
{
namespace
{
// 进程级:一个进程一种 etcd 传输。只在 Node 构造前(main 线程,单线程)写,之后只读。
Factory &InstalledFactory()
{
	static Factory factory;
	return factory;
}
} // namespace

void SetFactory(Factory factory)
{
	InstalledFactory() = std::move(factory);
}

std::unique_ptr<EtcdTransport> Create(muduo::net::EventLoop &loop, const std::string &endpoint, EtcdResponseSink &sink)
{
	if (const auto &factory = InstalledFactory())
	{
		if (auto transport = factory(loop, endpoint, sink))
		{
			return transport;
		}
	}
	return std::make_unique<GrpcEtcdTransport>(endpoint, sink);
}
} // namespace etcd_transport
