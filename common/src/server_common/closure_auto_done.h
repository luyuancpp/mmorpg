#ifndef COMMON_SRC_server_common_CLOSURE_AUTO_DONE_H_
#define COMMON_SRC_server_common_CLOSURE_AUTO_DONE_H_

#include <memory>

#include "google/protobuf/stubs/callback.h"

namespace common
{
    class AutoRecycleClosure
    {
    public:
		using element_type = ::google::protobuf::Closure;
        AutoRecycleClosure(element_type* done) :  done_(done) {}
        ~AutoRecycleClosure() { if (nullptr != done_) { done_->Run(); } }
        void SelfDelete() { done_ = nullptr; }//如果不自动回收的话就调用这个接口,让出管理权
    private:
        element_type* done_{nullptr};
    };

}//namespace common

#endif//COMMON_SRC_server_common_CLOSURE_AUTO_DONE_H_