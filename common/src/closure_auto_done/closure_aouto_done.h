#ifndef COMMON_SRC_CLOSURE_AUTO_DONE_CLOSURE_AUTO_DONE_H_
#define COMMON_SRC_CLOSURE_AUTO_DONE_CLOSURE_AUTO_DONE_H_

namespace google
{
    namespace protobuf
    {
        class Closure;
    }
}

namespace common
{
    class Closure_Deleter 
    {
    public:
        void operator()(::google::protobuf::Closure* c) { c->Run(); }
    };
    using ClosureAutoDone = std::unique_ptr<::google::protobuf::Closure, Closure_Deleter>;
}//namespace common

#endif//COMMON_SRC_CLOSURE_AUTO_DONE_CLOSURE_AUTO_DONE_H_
