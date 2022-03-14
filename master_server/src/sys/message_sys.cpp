#include "message_sys.hpp"

#include "src/common_type/common_type.h"
#include "src/game_logic/comp/gs_scene_comp.hpp"
#include "src/server_common/server_component.h"

using namespace common;

namespace master
{
	void Send2Gs(const google::protobuf::Message& message,
		const std::string& service,
		const std::string& method,
		uint32_t node_id)
{
		auto& gs_nodes = reg.get<GsNodes>(global_entity());
		auto it = gs_nodes.find(node_id);
		if (it == gs_nodes.end())
		{
			LOG_INFO << "gs not found ->" << node_id << "->" << service << "->" << method;
			return;
		}
		auto node =  reg.try_get<GsNodePtr>(it->second);
		if (nullptr == node)
		{
			LOG_INFO << "gs not found ->" << node_id << "->" << service << "->" << method;
			return;
		}
		(*node)->session_.Send(message, service, method);
}

void Send2GsPlayer(const google::protobuf::Message& message,
	const std::string& service,
	const std::string& method,
	entt::entity player_entity)
{
	auto ptr_gse = reg.try_get<GSEntity>(player_entity);
	if (nullptr == ptr_gse)
	{
		LOG_ERROR << "player send message empty server:" << reg.get<Guid>(player_entity)
			<< "message:" << message.GetTypeName();
		return;
	}
	auto& gs = reg.get<RpcServerConnection>(ptr_gse->server_entity());
	gs.Send(message, service, method);
}

}//namespace master