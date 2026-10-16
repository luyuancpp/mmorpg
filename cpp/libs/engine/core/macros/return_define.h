#pragma once

#include <cstdint>

#include <muduo/base/Logging.h>
#include <thread_context/ecs_context.h>

// todo.md #97: every rejected logic path must surface a user-facing tip
// AND leave a server-side breadcrumb. These macros are the call-site
// shorthand handlers use to return early on validation failure; before
// this change they silently dropped the error code, so an on-call had no
// way to tell "this RPC failed at precondition X" from "this RPC was
// never received". The LOG_WARN here is intentionally cheap (no proto
// serialization) — for full request-payload triage, the unified hook in
// error_handling.h + game_channel.cpp (#70/#125) takes over once the
// error reaches an actual RPC error response.

// Early-return when a result code signals failure.
// Wrapped in do-while(0) to behave as a single statement in all contexts.
#define RETURN_ON_ERROR(result)                                              \
    do {                                                                     \
        const auto _err = (result);                                          \
        if (_err != kSuccess) {                                              \
            LOG_WARN << "RETURN_ON_ERROR: " << __FUNCTION__                  \
                     << " (" << __FILE__ << ":" << __LINE__                  \
                     << ") code=" << _err;                                   \
            return _err;                                                     \
        }                                                                    \
    } while (0)

#define RETURN_FALSE_ON_ERROR(result)                                        \
    do {                                                                     \
        const auto _err2 = (result);                                         \
        if (_err2 != kSuccess) {                                             \
            LOG_WARN << "RETURN_FALSE_ON_ERROR: " << __FUNCTION__            \
                     << " (" << __FILE__ << ":" << __LINE__                  \
                     << ") code=" << _err2;                                  \
            return false;                                                    \
        }                                                                    \
    } while (0)

// Set error on protobuf response and return void.
#define SET_ERROR_IF_FAILURE(tip_code)                                       \
    do {                                                                    \
        const auto _code = (tip_code);                                      \
        if (_code != kSuccess) {                                            \
            LOG_WARN << "SET_ERROR_IF_FAILURE: " << __FUNCTION__            \
                     << " (" << __FILE__ << ":" << __LINE__                 \
                     << ") code=" << _code;                                 \
            response->mutable_error()->set_id(_code);                       \
            return;                                                         \
        }                                                                   \
    } while (0)

#define RETURN_IF_TRUE(condition, tip_code)                                  \
    do {                                                                    \
        if (condition) {                                                    \
            const auto _tip = (tip_code);                                   \
            LOG_WARN << "RETURN_IF_TRUE: " << __FUNCTION__                  \
                     << " (" << __FILE__ << ":" << __LINE__                 \
                     << ") code=" << _tip;                                  \
            return _tip;                                                    \
        }                                                                   \
    } while (0)

// 过渡兼容:生成头仍有调用;proto-gen 重生并确认无调用后删除本宏。
// 拒绝码规则见 services/player/rpc/player_service_interface.h,这里不再搬运任何状态。
#define TRANSFER_ERROR_MESSAGE(response) do { (void)(response); } while (0)

// 依赖调用处的 player / request / response;校验失败写本次应答并退出调用方。
// 拒绝码规则见 services/player/rpc/player_service_interface.h。
#define CHECK_PLAYER_REQUEST(request, fn)                                                       \
    do {                                                                                        \
        const auto _err4 = fn(player, request);                                                 \
        if (_err4 != kSuccess) {                                                                \
            LOG_WARN << "CHECK_PLAYER_REQUEST: " << __FUNCTION__                                \
                     << " (" << __FILE__ << ":" << __LINE__                                     \
                     << ") code=" << _err4;                                                     \
            response->mutable_error_message()->set_id(_err4);                                   \
            return;                                                                             \
        }                                                                                       \
    } while (0)

// ─── ECS Component Guard Macros ──────────────────────────────────────
//
// try_get a component pointer from tlsEcs.actorRegistry, early-exit if null.
// Same convention as the generated Lookup* macros:
//   variable is declared in the enclosing scope, guard runs in do-while(0).
//
// Usage:
//   ECS_GET_OR_VOID(buffList, BuffListComp, entity);     // returns void
//   ECS_GET_OR_RETURN(state, ActorStateComp, e, kSuccess); // returns custom value
//   ECS_GET_OR_CONTINUE(comp, Transform, entity);        // continue in loop
//   ECS_GET_OR_FALSE(attr, BaseAttributesComp, entity);  // returns false
// ─────────────────────────────────────────────────────────────────────

#define ECS_GET_OR_RETURN(var, Type, entity, retval)        \
    auto *var = tlsEcs.actorRegistry.try_get<Type>(entity); \
    do                                                      \
    {                                                       \
        if (!(var))                                         \
            return (retval);                                \
    } while (0)

#define ECS_GET_OR_VOID(var, Type, entity)                  \
    auto *var = tlsEcs.actorRegistry.try_get<Type>(entity); \
    do                                                      \
    {                                                       \
        if (!(var))                                         \
            return;                                         \
    } while (0)

#define ECS_GET_OR_CONTINUE(var, Type, entity)              \
    auto *var = tlsEcs.actorRegistry.try_get<Type>(entity); \
    do                                                      \
    {                                                       \
        if (!(var))                                         \
            continue;                                       \
    } while (0)

#define ECS_GET_OR_FALSE(var, Type, entity)                 \
    auto *var = tlsEcs.actorRegistry.try_get<Type>(entity); \
    do                                                      \
    {                                                       \
        if (!(var))                                         \
            return false;                                   \
    } while (0)
