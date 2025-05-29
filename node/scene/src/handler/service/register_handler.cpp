
#include <unordered_map>
#include <memory>
#include <google/protobuf/service.h>

extern std::unordered_map<std::string, std::unique_ptr<::google::protobuf::Service>> gNodeService;

void InitServiceHandler()
{
}
