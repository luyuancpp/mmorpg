#pragma once

#include <cstdint>

#include <thread_context/ecs_context.h>

// Early-return when a result code signals failure.
// Wrapped in do-while(0) to behave as a single statement in all contexts.
#define RETURN_ON_ERROR(result)              \
    do {                                     \
        const auto _err = (result);          \
        if (_err != kSuccess) return _err;   \
    } while (0)

#define RETURN_FALSE_ON_ERROR(result)        \
    do {                                     \
        if ((result) != kSuccess) return false; \
    } while (0)

// Set error on protobuf response and return void.
#define SET_ERROR_IF_FAILURE(tip_code)                  \
    do {                                                \
        const auto _code = (tip_code);                  \
        if (_code != kSuccess) {                        \
            response->mutable_error()->set_id(_code);   \
            return;                                     \
        }                                               \
    } while (0)

#define RETURN_IF_TRUE(condition, tip_code)   \
    do {                                      \
        if (condition) return (tip_code);     \
    } while (0)

// Transfer accumulated TipInfoMessage to a protobuf response and clear TLS state.
#define TRANSFER_ERROR_MESSAGE(response)                                                        \
    do {                                                                                        \
        auto* _resp = (response);                                                               \
        if (_resp) {                                                                            \
            auto& _tip = tlsEcs.globalRegistry.get_or_emplace<TipInfoMessage>(      \
                tlsEcs.GlobalEntity());                                                                \
            *(_resp->mutable_error_message()) = std::move(_tip);                                \
            _tip.Clear();                                                                       \
        }                                                                                       \
    } while (0)

// Validate a request through |fn|; on failure, record error in TLS and return void.
#define CHECK_REQUEST_PRECONDITIONS(request, fn)                                                \
    do {                                                                                        \
        const auto _err = fn(request);                                                          \
        if (_err != kSuccess) {                                                                 \
            tlsEcs.globalRegistry.get_or_emplace<TipInfoMessage>(tlsEcs.GlobalEntity())    \
                .set_id(_err);                                                                  \
            return;                                                                             \
        }                                                                                       \
    } while (0)

// Validate a player request through |fn(player, request)|.
#define CHECK_PLAYER_REQUEST(request, fn)                                                       \
    do {                                                                                        \
        const auto _err = fn(player, request);                                                  \
        if (_err != kSuccess) {                                                                 \
            tlsEcs.globalRegistry.get_or_emplace<TipInfoMessage>(tlsEcs.GlobalEntity())    \
                .set_id(_err);                                                                  \
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
