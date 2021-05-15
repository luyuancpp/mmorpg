#ifndef COMMON_SRC_RETURN_CODE_NoticeStruct
#define COMMON_SRC_RETURN_CODE_NoticeStruct

#include "src/common_type/common_type.h"

namespace common
{
    class ReturnNoticeStruct
    {
    public:
        using ErrorNum = uint32_t;
        ReturnNoticeStruct(ErrorNum num) : num_(num){}
        ErrorNum num() const { return num_; }
        void AddParam(const std::string& p) { param_.push_back(p); }
    private:
        ErrorNum num_{ 0 };
        StringV param_;
    };
}//namespace common

using ReturnValue = common::ReturnNoticeStruct;

#endif//COMMON_SRC_RETURN_CODE_NoticeStruct 