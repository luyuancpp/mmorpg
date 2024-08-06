#pragma once

#include <cstdint>

#define CHECK_RETURN_IF_NOT_OK(f)\
{\
    uint32_t ret(f);\
    if (ret != kOK)\
    {\
        return ret;\
    }\
}

#define SET_ERROR_AND_RETURN_IF_NOT_OK(tip_code)\
if ((tip_code) != kOK)\
{\
    response->mutable_error()->set_id(tip_code);\
    return;\
}

#define CHECK_CONDITION(condition, tip_code)\
if (condition)\
{\
    return tip_code; \
}


