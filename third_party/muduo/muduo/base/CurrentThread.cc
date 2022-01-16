// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/base/CurrentThread.h"
#ifdef __linux__
#include <cxxabi.h>
#endif// __linux__
#include <execinfo.h>
#include <stdlib.h>

namespace muduo
{
namespace CurrentThread
{
__thread int t_cachedTid = 0;
__thread char t_tidString[32];
__thread int t_tidStringLength = 6;
__thread const char* t_threadName = "unknown";
static_assert(std::is_same<int, pid_t>::value, "pid_t should be int");

string stackTrace(bool demangle)
{
  string stack;
#ifdef __linux__
  const int max_frames = 200;
  void* frame[max_frames];
  int nptrs = ::backtrace(frame, max_frames);
  char** strings = ::backtrace_symbols(frame, nptrs);
  if (strings)
  {
    size_t len = 256;
    char* demangled = demangle ? static_cast<char*>(::malloc(len)) : nullptr;
    for (int i = 1; i < nptrs; ++i)  // skipping the 0-th, which is this function
    {
      if (demangle)
      {
        // https://panthema.net/2008/0901-stacktrace-demangled/
        // bin/exception_test(_ZN3Bar4testEv+0x79) [0x401909]
        char* left_par = nullptr;
        char* plus = nullptr;
        for (char* p = strings[i]; *p; ++p)
        {
          if (*p == '(')
            left_par = p;
          else if (*p == '+')
            plus = p;
        }

        if (left_par && plus)
        {
          *plus = '\0';
          int status = 0;
          char* ret = abi::__cxa_demangle(left_par+1, demangled, &len, &status);
          *plus = '+';
          if (status == 0)
          {
            demangled = ret;  // ret could be realloc()
            stack.append(strings[i], left_par+1);
            stack.append(demangled);
            stack.append(plus);
            stack.push_back('\n');
            continue;
          }
        }
      }
      // Fallback to mangled names
      stack.append(strings[i]);
      stack.push_back('\n');
    }
    free(demangled);
    free(strings);
  }
#else
  const int len = 200;
  const int maxFunNameLen = 512;
  void         * buffer[len];
  HANDLE         process = GetCurrentProcess();

  SymInitialize(process, nullptr, true);

  int nptrs = CaptureStackBackTrace(0, len, buffer, nullptr);

  SYMBOL_INFO  * symbol = (SYMBOL_INFO *)calloc(sizeof(SYMBOL_INFO) + maxFunNameLen * sizeof(char), 1);
  symbol->MaxNameLen = maxFunNameLen;
  symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

  if (symbol)
  {
      for (int i = 0; i < nptrs; ++i)
      {
          SymFromAddr(process, (DWORD64)(buffer[i]), 0, symbol);
          char funstring[maxFunNameLen] = {};
          sprintf_s(funstring, "%i: %s - 0x%I64X\n", nptrs - i - 1, symbol->Name, symbol->Address);
          stack.append(funstring);
      }
      free(symbol);
  }

#endif// __linux__
  return stack;
}

}  // namespace CurrentThread
}  // namespace muduo
