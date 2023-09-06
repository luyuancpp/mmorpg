#pragma once

#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/game_logic/enum/server_enum.h"

#include "src/common_type/common_type.h"

inline static constexpr std::size_t kMaxServerPlayerSize = 2000;
inline static constexpr std::size_t kMaxScenePlayerSize = 1000;

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
		return player_ == entt::null || dest_node_ == entt::null;
	}
    entt::entity player_{ entt::null };
    entt::entity dest_node_{ entt::null };
    uint32_t scene_confid_{0};
};

class ServerNodeSystem
{
public:

	//得到有该场景的人数最少的服务器
	static entt::entity GetMinPlayerSizeServerScene(const GetSceneParam& param);

	//得到有该场景的人数不满的服务器
    static  entt::entity GetSceneServerNotFull(const GetSceneParam& param);

    static  void ServerEnterPressure(const ServerPressureParam& param);

    static  void ServerEnterNoPressure( const ServerPressureParam& param);

    static  void set_server_state(const ServerStateParam& param);

};

