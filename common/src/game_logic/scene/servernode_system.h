#pragma once

#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/game_logic/enum/server_enum.h"

#include "src/common_type/common_type.h"

static const std::size_t kMaxServerPlayerSize = 2000;
static const std::size_t kMaxScenePlayerSize = 1000;

struct GetSceneParam
{
	uint32_t scene_confid_{ 0 };
};

struct GetSceneFilterParam
{
	ServerPressureState server_pressure_state_{ ServerPressureState::kNoPressure };
};

struct ServerPressureParam
{
	inline bool IsNull() const
	{
		return server_ == entt::null;
	}
    entt::entity server_{ entt::null };
};

struct ServerStateParam
{
	inline bool IsNull() const
	{
		return node_entity_ == entt::null;
	}
    entt::entity node_entity_{ entt::null };
	ServerState server_state_{ ServerState::kNormal };
};

struct ReplaceCrashServerParam
{
	inline bool IsNull() const
	{
		return cransh_server_ == entt::null || replace_server_ == entt::null;
	}
    entt::entity cransh_server_{ entt::null };
	entt::entity replace_server_{ entt::null };
};

struct CompelChangeSceneParam
{
	inline bool IsNull() const
	{
		return player_ == entt::null || new_server_ == entt::null;
	}
    entt::entity player_{ entt::null };
    entt::entity new_server_{ entt::null };
    uint32_t scene_confid_{0};
};

class ServerNodeSystem
{
public:

	static entt::entity GetWeightRoundRobinMainScene(const GetSceneParam& param);

    static  entt::entity GetWeightRoundRobinRoomScene(const GetSceneParam& param);

    static  entt::entity GetMainSceneNotFull(const GetSceneParam& param);

    static  void ServerEnterPressure(const ServerPressureParam& param);

    static  void ServerEnterNoPressure( const ServerPressureParam& param);

    static  void set_server_state(const ServerStateParam& param);

};

