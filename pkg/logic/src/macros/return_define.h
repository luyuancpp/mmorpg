#pragma once

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
 } while (0)

#define SET_ERROR_AND_RETURN_IF_NOT_OK(tip_code)\
 do { \
    if ((tip_code) != kOK)\
    {\
        response->mutable_error()->set_id(tip_code);\
        return;\
    }\
 } while (0)

#define CHECK_CONDITION(condition, tip_code)\
 do { \
    if (condition)\
    {\
        return tip_code; \
    }\
 } while (0)


#define HANDLE_ERROR_MESSAGE(response) \
    do { \
        auto* tipInfoMessage = tls.globalRegistry.try_get<TipInfoMessage>(GlobalEntity()); \
        if (tipInfoMessage != nullptr) { \
            *(response)->mutable_error_message() = std::move(*tipInfoMessage); \
            tls.globalRegistry.remove<TipInfoMessage>(GlobalEntity()); \
        } \
    } while (0)


