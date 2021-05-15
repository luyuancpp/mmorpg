#ifndef COMMON_SRC_RETURN_CODE_NoticeStruct
#define COMMON_SRC_RETURN_CODE_NoticeStruct

#include "src/common_type/common_type.h"

namespace common
{
    enum  class EnumCode : uint32_t;

    class ReturnNoticeStruct
    {
    public:
        using ErrorNum = uint32_t;
        ReturnNoticeStruct(EnumCode num) : num_(static_cast<ErrorNum>(num)) {}
        operator common::EnumCode() const { return common::EnumCode(num_); }
        operator common::EnumCode(){ return common::EnumCode(num_); }
        bool operator!=(common::EnumCode c) const { return num() != ErrorNum(c); }
        bool operator!=(common::EnumCode c) { return num() != ErrorNum(c); }
        bool operator!=(const ReturnNoticeStruct& rv) const { return num() != rv.num(); }
        bool operator!=(const ReturnNoticeStruct& rv) { return num() != rv.num(); }
        ErrorNum num() const { return num_; }

        void AddParam(const std::string& p) { param_.push_back(p); }
       
    private:
        ErrorNum num_{ 0 };
        StringV param_;
    };
}//namespace common

using ReturnValue = common::ReturnNoticeStruct;

bool operator!=(common::EnumCode l, const ReturnValue& r);

#endif//COMMON_SRC_RETURN_CODE_NoticeStruct 