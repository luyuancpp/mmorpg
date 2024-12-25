#include <cstdint>

#include "thread_local/storage_lua.h"
#include "proto/logic/client_player/player_skill.pb.h"


void InitPlayerSkillServiceLua()
{
	tls_lua_state["PlayerSkillServiceReleaseSkillMessageId"] = 18;
	tls_lua_state["PlayerSkillServiceReleaseSkillIndex"] = 0;
	tls_lua_state["PlayerSkillServiceReleaseSkill"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return PlayerSkillService_Stub::descriptor()->method(0);
	};

	tls_lua_state["PlayerSkillServiceNotifySkillUsedMessageId"] = 49;
	tls_lua_state["PlayerSkillServiceNotifySkillUsedIndex"] = 1;
	tls_lua_state["PlayerSkillServiceNotifySkillUsed"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return PlayerSkillService_Stub::descriptor()->method(1);
	};

	tls_lua_state["PlayerSkillServiceNotifySkillInterruptedMessageId"] = 58;
	tls_lua_state["PlayerSkillServiceNotifySkillInterruptedIndex"] = 2;
	tls_lua_state["PlayerSkillServiceNotifySkillInterrupted"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return PlayerSkillService_Stub::descriptor()->method(2);
	};

	tls_lua_state["PlayerSkillServiceGetSkillListMessageId"] = 57;
	tls_lua_state["PlayerSkillServiceGetSkillListIndex"] = 3;
	tls_lua_state["PlayerSkillServiceGetSkillList"] = []()-> const ::google::protobuf::MethodDescriptor* {
		return PlayerSkillService_Stub::descriptor()->method(3);
	};

}
