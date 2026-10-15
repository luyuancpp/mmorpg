#include "node_context_manager.h"

#include <cstdlib>

#include "muduo/base/Logging.h"

thread_local NodeContextManager tlsNodeContextManager;

void NodeContextManager::FailOutOfRange(const char* caller, uint32_t nodeType)
{
	// 越界说明某个调用方把未校验的 nodeType 当成了可信值:外部输入漏过了 TryGetRegistry,
	// 或者内部算错了类型。继续跑只会把副作用落到错误的地方,这里当场终止,日志里留下调用点和值。
	LOG_FATAL << "NodeContextManager::" << caller << ": nodeType=" << nodeType
			  << " out of range [0, " << kNodeTypeCount << ")";
	// LOG_FATAL 在日志对象析构时就会 abort();这里再兜一次,让 [[noreturn]] 不依赖日志库的实现。
	std::abort();
}
