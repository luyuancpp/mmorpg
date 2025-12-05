#pragma once

#include <cstdint>

#include <threading/registry_manager.h>

entt::entity GlobalEntity();

#define RETURN_ON_ERROR(result)  \
if ((result) != kSuccess) {   \
return (result);          \
}

#define RETURN_FALSE_ON_ERROR(result) \
if ((result) != kSuccess) {           \
return false;                     \
}

#define SET_ERROR_IF_FAILURE(tip_code) \
if ((tip_code) != kSuccess) { \
response->mutable_error()->set_id(tip_code); \
return; \
}

#define RETURN_IF_TRUE(condition, tip_code) \
if (condition) { \
return tip_code; \
}

#define TRANSFER_ERROR_MESSAGE(response) \
auto resp = response;\
if (resp) { \
	auto& tipInfoMessage = tlsRegistryManager.globalRegistry.get_or_emplace<TipInfoMessage>(GlobalEntity()); \
	*(resp->mutable_error_message()) = std::move(tipInfoMessage); \
	tipInfoMessage.Clear(); \
} 

#define CHECK_REQUEST_PRECONDITIONS(request, fn) \
{ \
auto err = fn(request); \
if (err != kSuccess) { \
tlsRegistryManager.globalRegistry.get_or_emplace<TipInfoMessage>(GlobalEntity()).set_id(err); \
return; \
} \
}

#define CHECK_PLAYER_REQUEST(request, fn) \
{ \
auto err = fn(player, request); \
if (err != kSuccess) { \
tlsRegistryManager.globalRegistry.get_or_emplace<TipInfoMessage>(GlobalEntity()).set_id(err); \
return; \
} \
}
