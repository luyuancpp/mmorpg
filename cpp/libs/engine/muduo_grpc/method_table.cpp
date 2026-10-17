#include "method_table.h"

#include <algorithm>

#include "muduo/base/Logging.h"

namespace muduo_grpc
{

std::optional<std::pair<std::string_view, std::string_view>> SplitMethodPath(std::string_view path)
{
    if (path.size() < 2 || path.front() != '/')
    {
        return std::nullopt;
    }
    const std::string_view rest = path.substr(1);
    const size_t slash = rest.find('/');
    if (slash == std::string_view::npos)
    {
        return std::nullopt;
    }
    const std::string_view service = rest.substr(0, slash);
    const std::string_view method = rest.substr(slash + 1);
    if (service.empty() || method.empty() || method.find('/') != std::string_view::npos)
    {
        return std::nullopt;
    }
    return std::make_pair(service, method);
}

bool MethodTable::AddUnary(std::string path, UnaryMethodHandler handler)
{
    const auto parts = SplitMethodPath(path);
    if (!parts)
    {
        LOG_ERROR << "grpc method table: malformed method path '" << path << "'";
        valid_ = false;
        return false;
    }
    if (!handler)
    {
        LOG_ERROR << "grpc method table: empty handler for '" << path << "'";
        valid_ = false;
        return false;
    }
    if (methods_.find(std::string_view(path)) != methods_.end())
    {
        LOG_ERROR << "grpc method table: duplicate method '" << path << "'";
        valid_ = false;
        return false;
    }
    services_.emplace(parts->first);
    // parts 指向 path,必须在 path 被移走之前用完。
    methods_.emplace(std::move(path), std::move(handler));
    return true;
}

const UnaryMethodHandler *MethodTable::Find(std::string_view path) const
{
    const auto it = methods_.find(path);
    return it == methods_.end() ? nullptr : &it->second;
}

bool MethodTable::HasService(std::string_view fullServiceName) const
{
    return services_.find(fullServiceName) != services_.end();
}

std::vector<std::string> MethodTable::Paths() const
{
    std::vector<std::string> paths;
    paths.reserve(methods_.size());
    for (const auto &entry : methods_)
    {
        paths.push_back(entry.first);
    }
    std::sort(paths.begin(), paths.end());
    return paths;
}

} // namespace muduo_grpc
