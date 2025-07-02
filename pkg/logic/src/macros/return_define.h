#pragma once

#include <cstdint>

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
if (auto* tipInfoMessage = tls.globalRegistry.try_get<TipInfoMessage>(GlobalEntity())) { \
if (response) { \
*(response)->mutable_error_message() = std::move(*tipInfoMessage); \
tls.globalRegistry.remove<TipInfoMessage>(GlobalEntity()); \
} \
}

#define CHECK_REQUEST_PRECONDITIONS(request, fn) \
{ \
auto err = fn(request); \
if (err != kSuccess) { \
tls.globalRegistry.get_or_emplace<TipInfoMessage>(GlobalEntity()).set_id(err); \
return; \
} \
}

#define CHECK_PLAYER_REQUEST(request, fn) \
{ \
auto err = fn(player, request); \
if (err != kSuccess) { \
tls.globalRegistry.get_or_emplace<TipInfoMessage>(GlobalEntity()).set_id(err); \
return; \
} \
}
