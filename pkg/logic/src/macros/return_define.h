﻿#pragma once

#include <cstdint>

#define CHECK_RETURN_IF_NOT_OK(f)\
 do { \
    {\
        uint32_t ret(f);\
        if (ret != kOK)\
        {\
            return ret;\
        }\
    }\
 } while (false)

#define SET_ERROR_AND_RETURN_IF_NOT_OK(tip_code)\
 do { \
    if ((tip_code) != kOK)\
    {\
        response->mutable_error()->set_id(tip_code);\
        return;\
    }\
 } while (false)

#define CHECK_CONDITION(condition, tip_code)\
 do { \
    if (condition)\
    {\
        return tip_code; \
    }\
 } while (false)


#define HANDLE_ERROR_MESSAGE(response) \
    do { \
        auto* tipInfoMessage = tls.globalRegistry.try_get<TipInfoMessage>(GlobalEntity()); \
        if (tipInfoMessage != nullptr && response != nullptr) { \
            *(response)->mutable_error_message() = std::move(*tipInfoMessage); \
            tls.globalRegistry.remove<TipInfoMessage>(GlobalEntity()); \
        } \
    } while (false)

// 定义宏以检查技能激活前提条件
#define CHECK_REQUEST(request, fn) \
do { \
auto err = fn(request); \
if (err != kOK) \
{ \
tls.globalRegistry.emplace_or_replace<TipInfoMessage>(GlobalEntity()).set_id(err); \
return;\
} \
} while (false)

// 定义宏以检查技能激活前提条件
#define CHECK_PLAYER_REQUEST(request, fn) \
do { \
auto err = fn(player, request); \
if (err != kOK) \
{ \
tls.globalRegistry.emplace_or_replace<TipInfoMessage>(GlobalEntity()).set_id(err); \
return;\
} \
} while (false)