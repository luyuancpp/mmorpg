# 游戏服务器线上崩溃和内存问题排查问答

整理日期：2026-10-18。用于准备 C++、Go、Java 服务端面试，以及理解崩溃现场与内存破坏根因之间的关系。核心顺序是：确认退出原因，保留现场，验证堆栈可信度，缩小可疑写入或异常持有范围，再通过复现和回归确认根因。

文中的 C++ 程序、地址、汇编和调试输出是教学示意，未在当前环境编译运行，也不代表本项目发生过对应事故。示例按 Linux x86-64、GCC、保留帧指针的条件解释；真实构建的优化、栈保护、控制流保护和插桩可能改变现场，所有偏移必须从事故二进制确认。

## 面试口述稿

> 我先确认进程是因为段错误、断言、未恢复的 panic、JVM 致命错误，还是 OOM kill、外部信号而退出，同时保留事故版本的二进制、动态库、符号和转储。
>
> 堆栈异常时，先排除符号错配、展开信息不足和线程选择问题。确认栈被覆盖后，从当前指令、寄存器和原始栈内存确定破坏范围，再查对象生命周期、长度来源和相关写入点。崩在返回指令、malloc 或 free，根因可能是更早发生的越界或释放后使用。
>
> core 只能提供崩溃时的快照。我会用相同输入复现，通过 ASan 或硬件写监视点抓非法写入；并发问题另外用竞态检测。排查内存泄漏时，先区分存活对象增长、分配器保留和堆外增长，再查分配来源与持有关系，最后用边界和失败场景回归验证。

## 先判断退出原因与堆栈可信度

先看应用日志、退出状态以及系统和容器记录。OOM kill 或外部强制终止可能没有应用异常、调用栈或 core，不能只检查应用日志。保留现场后，根据故障影响安排摘流量、回滚或重启。

分析转储需要保留事故进程对应的可执行文件、动态库、独立调试符号及版本信息。相同源码重新编译的程序不能自动替代事故程序；Linux 可结合 Build ID 核对符号，Windows 则需要匹配的 EXE、DLL 和 PDB。GDB 的符号匹配机制见[独立调试文件说明](https://www.sourceware.org/gdb/current/onlinedocs/gdb.html/Separate-Debug-Files.html)。

“堆栈是错的”至少有三类含义：

| 现象 | 检查方向 |
| --- | --- |
| 函数名或行号与部署代码对不上 | 二进制、动态库、调试符号、源码版本和加载地址是否匹配 |
| 帧缺失、局部变量不可见、调用关系难读 | 优化、内联、尾调用、展开信息、调试器支持及转储完整性 |
| 返回地址或帧数据出现输入内容、无法合理展开 | 先验证符号和布局，再考虑越界覆盖、释放后使用或其他内存破坏 |

缺少帧指针不等于一定无法展开，调试器还可能使用展开信息；保留帧指针也不能恢复已经被覆盖的返回地址。异步任务、消息队列和回调的提交方通常不在执行方的当前调用栈中，需要关联日志或追踪信息补齐业务因果链。

## C++ 栈越界覆盖返回地址的具体案例

### 构造有问题的输入与复制代码

下面的结构仅用于独立教学程序，不是本项目的协议或 handler：

```cpp
#include <cstddef>
#include <cstring>

struct LoginPacket {
    std::size_t name_len;
    char name[64];
};

LoginPacket g_packet{
    32,
    "AAAAAAAAAAAAAAAABBBBBBBBCCCCCCCC"
};

void HandleLogin(const LoginPacket& packet)
{
    char name[16];

    // 长度来自输入，复制前未检查目标容量。
    std::memcpy(name, packet.name, packet.name_len);
}

int main()
{
    HandleLogin(g_packet);
}
```

输入是 16 个 A、8 个 B、8 个 C，总长 32 字节。目标局部数组只有 16 字节；复制操作越过数组边界后，函数可能继续执行，直到返回时才暴露错误。

为了说明覆盖返回地址后的现场，本节假设教学构建未启用栈保护。生产保留栈保护；如果保护值先被破坏，进程可能在 `__stack_chk_fail` 中终止，此时需分析调用该检查的业务函数，不能直接套用下面停在 `ret` 的寄存器关系。见 [GCC 栈保护选项](https://gcc.gnu.org/onlinedocs/gcc/Instrumentation-Options.html)。

### 打开 core 并确认当前故障指令

匹配事故二进制与符号后执行：

```gdb
gdb ./demo ./core
thread apply all bt full
info registers rip rsp rbp
x/i $pc
```

多线程服务先确认故障线程。下面只展开它的示意现场：

```text
#0  HandleLogin(...) at demo.cpp:...
#1  0x4343434343434343 in ?? ()

rip = 0x401170             <HandleLogin 中的 ret>
rsp = 0x7fffffffe118
rbp = 0x4242424242424242

=> 0x401170: ret
```

RIP 表示当前指令位置，RSP 是当前栈顶，RBP 在本构建的函数执行期间用作帧基址。`0x42` 是字符 B，`0x43` 是字符 C，寄存器与回溯中的重复内容提示输入可能覆盖了帧数据，但尚未证明具体写入者。

`0x4343434343434343` 是非 canonical 地址。在这个示例条件下，CPU 尝试执行 `ret` 时可因非法返回目标触发异常，RIP 仍停在有效的 `ret` 指令地址。不要将现场统一写成 RIP 已跳到 `0x4343...`；如果返回目标是 canonical 但未映射的地址，则可能在目标取指时异常。应以故障指令和实际寄存器为准。

### 从反汇编确认数组和返回地址槽的位置

```gdb
set disassembly-flavor intel
disassemble /r HandleLogin
```

假设事故二进制中关键指令如下，参数装载部分省略：

```asm
push rbp
mov  rbp, rsp
sub  rsp, 0x20

; 根据实际指令确认 name 位于 rbp - 0x10
lea  rax, [rbp-0x10]
...
call memcpy@plt

leave
ret
```

根据这份反汇编，函数执行期间的布局为：

| 相对原 RBP 的位置 | 内容 | 大小 |
| --- | --- | --- |
| 原 RBP − 16 | 局部数组 name | 16 字节 |
| 原 RBP | 保存的调用者 RBP | 8 字节 |
| 原 RBP ＋ 8 | 返回地址槽 | 8 字节 |

因此，该构建中从 name 起点连续写入 32 字节，会按顺序覆盖数组、保存的 RBP 和返回地址。GCC 的 `-O0` 也不保证这个布局，源码中的声明顺序不能代替反汇编证据。GDB 命令见[源码与机器码说明](https://www.sourceware.org/gdb/current/onlinedocs/gdb.html/Machine-Code.html)。

### RBP 已被覆盖时用 RSP 定位原始内存

本示例已执行 `leave`，停在 `ret`。`leave` 先用该函数原来的 RBP 恢复 RSP，再从栈中弹出保存的调用者 RBP。弹出的值已被 B 覆盖，所以当前 RBP 已经失效；此时 RSP 仍指向待使用的返回地址槽。

结合刚才核对的布局，才能推导：

```text
返回地址槽  = 当前 RSP
保存 RBP 槽 = 当前 RSP − 8
name 起点   = 当前 RSP − 24
```

读取内存：

```gdb
x/4gx $rsp-24
```

示意输出：

```text
0x7fffffffe100: 0x4141414141414141  0x4141414141414141
0x7fffffffe110: 0x4242424242424242  0x4343434343434343
```

| 地址范围 | 原用途 | 当前内容 |
| --- | --- | --- |
| 0x7fffffffe100 至 0x7fffffffe10f | name 数组 | 16 个 A |
| 0x7fffffffe110 至 0x7fffffffe117 | 保存的 RBP | 8 个 B |
| 0x7fffffffe118 至 0x7fffffffe11f | 返回地址 | 8 个 C |

此时不能用 `$rbp-16` 找数组，因为当前 RBP 已损坏。`$rsp-24` 也不是通用公式：只有布局、函数尾声和停机位置全部符合上述条件才能使用。GDB `x/4gx` 表示按十六进制读取 4 个 8 字节单位，见[原始内存读取说明](https://www.sourceware.org/gdb/current/onlinedocs/gdb.html/Memory.html)。

### 将覆盖内容连到输入和可疑写入点

教学程序中的请求保存在未被此次覆盖破坏的全局变量里，可以检查：

```gdb
p g_packet.name_len
x/s g_packet.name
```

对应的示意值是：

```text
32
"AAAAAAAAAAAAAAAABBBBBBBBCCCCCCCC"
```

现有证据能串起以下关系：返回指令异常；返回地址槽被输入中的 C 覆盖；从数组到返回地址槽的字节排列与请求一致；业务代码使用输入长度向 16 字节数组复制；保存的教学输入长度为 32。

这使该复制调用成为高度可疑的写入点。真实服务仍须确认请求对应本次调用、请求保存区没有被修改、复制长度与输入长度的转换关系，并搜索所有可能写入该内存的路径。如果存在多个写入者，不能仅凭内容相同就认定历史写入指令。

也不能直接把 `ret` 时的 RDX 当成原始复制长度：Linux x86-64 常见调用约定下，相关参数寄存器可由被调函数修改，返回现场不保证保留调用前值。

### 重新运行并监视返回地址槽

普通 core 没有完整执行历史，不能在它上面设置监视点后继续执行以找回过去的写入。用相同输入重新运行：

```gdb
gdb ./demo
break HandleLogin
run
```

确认函数序言已执行、RBP 正常，并且尚未执行复制语句。根据断点实际位置检查源码与指令；需要时推进到复制调用前。先检查输入与目标容量：

```gdb
p packet.name_len
p sizeof(name)
```

在已确认上述函数布局的前提下，计算本次运行的返回地址槽并监视：

```gdb
set $slot = (char*)$rbp + 8
watch -location *(unsigned long long*)$slot
continue
```

确认 GDB 显示 `Hardware watchpoint`。当返回地址槽被改写时，预期可以观察到正常返回地址变成 `0x4343434343434343`。此时执行 `bt`，暂停位置可能在 libc 的 memcpy 实现里，上层调用点应对应 `HandleLogin` 的复制语句。

x86 硬件数据监视点通常在写入完成后报告；应结合附近指令和调用点判断写入位置。每次运行都重新计算地址，不能直接复用 core 中的绝对地址。该方法捕获的是改写返回地址槽的操作；如果要追查更早的越界，可监视数组边界后的内存，或使用 ASan。`watch` 只在被监视值改变时报告，单个边界地址也不能覆盖所有越界访问。见 [GDB 监视点说明](https://www.sourceware.org/gdb/current/onlinedocs/gdb.html/Set-Watchpoints.html)。

### 用 ASan 捕获数组越界并验证修复

将教学代码另存为独立目录中的 `demo.cpp` 后，可使用以下命令构建复现版本，不修改服务端正式构建：

```bash
g++ -g -O0 -fsanitize=address \
    -fno-omit-frame-pointer demo.cpp -o demo_asan
./demo_asan
```

预期在复制时报告 `stack-buffer-overflow`，指向业务复制调用以及局部数组 name。这里使用 `-O0`，避免极小示例中没有后续可观察用途的复制被优化掉。ASan 会改变栈布局，不能把插桩版本的偏移套回原 core。检测范围和编译方式见 [ASan 官方文档](https://clang.llvm.org/docs/AddressSanitizer.html)。

根因应表述为“未经目标容量校验的输入长度进入复制调用”。修复时按协议校验源数据实际可读长度和目标容量；若 name 作为 C 字符串使用，还须预留并写入终止符。回归至少覆盖空输入、最大合法长度、刚好越过合法上界以及本例的 32 字节输入。生产问题涉及并发时，还要覆盖生命周期和竞争条件。

证据应分开记录：core 已证明哪些内存被破坏，哪些写入点仍是推断，复现工具实际捕获了什么，修复后哪些用例通过。只有 core 时不能把尚未执行的复现写成已确认的历史写入。

## C++ Go Java 的排查差异

| 问题 | C++ | Go | Java |
| --- | --- | --- | --- |
| 普通数组越界 | 通常属于未定义行为，可能立即崩溃，也可能稍后暴露 | 数组和 slice 越界触发运行时 panic | 数组越界抛出 ArrayIndexOutOfBoundsException |
| 进程退出 | 查信号、访问冲突、断言、转储和生命周期 | 区分未恢复 panic、runtime fatal error、cgo/native 问题及外部终止 | 区分线程中的未捕获异常、OOM、JVM 致命崩溃及外部终止 |
| 怀疑内存破坏 | 分析 core，按问题使用 ASan、UBSan、独立 TSan 构建或写监视点 | 查 unsafe、cgo 和数据竞争，结合 Go 与 native 调试器 | JVM 崩溃时保留 hs_err_pid 日志、core、准确 JDK 和 native 库，检查 JNI、Unsafe 或 JVM 问题 |

Go 未恢复的 panic 可结束进程；Java 中未捕获的普通异常通常终止当前线程，是否进一步导致进程退出还要看线程和进程状态。两者的边界检查语义分别见 [Go 规范](https://go.dev/ref/spec#Index_expressions)和 [Java 规范](https://docs.oracle.com/javase/specs/jls/se25/html/jls-15.html#jls-15.10.4)。

Go 可通过 `GOTRACEBACK=system` 增加 runtime 和其他 goroutine 信息；`GOTRACEBACK=crash` 在支持并允许转储的环境中触发系统级崩溃转储。分析 Go 状态可用 Delve，涉及 cgo 时结合 native 调试器。配置语义见 [Go runtime 文档](https://pkg.go.dev/runtime#hdr-Environment_Variables)。

纯 Go 也不能忽略数据竞争：slice、interface 等多字结构在竞争下可能出现不一致状态并进一步破坏内存。`-race` 只发现实际执行中发生的竞争，未报错不能证明所有路径没有竞态，见 [Go 内存模型](https://go.dev/ref/mem)与[竞态检测说明](https://go.dev/doc/articles/race_detector)。

## 内存持续上涨如何判断是不是泄漏

先在相近负载与可比 GC 状态下对比多个时间点。RSS 持续上涨可能来自存活对象、内存池保留、碎片、线程栈、映射或 native 分配，单凭 RSS 曲线不能认定泄漏。OOM 也可能来自容量不足、突发大分配或容器限制。

| 语言 | 定位方法 | 常见检查点与解释边界 |
| --- | --- | --- |
| C++ | LSan、堆分配跟踪或分配器剖析，比较持续未释放的分配 | 所有权、错误路径、循环 shared_ptr、无界缓存；仍可达但不再需要的对象也可能形成逻辑泄漏，不能只靠泄漏检测器 |
| Go | 比较多个 heap profile 的 inuse_space、inuse_objects，同时观察 goroutine | 无界 map/cache、消息积压、阻塞 goroutine、小 slice 持有大底层数组；alloc_space 大代表累计分配多，heap profile 主要提供分配栈，不能直接当作对象引用链 |
| Java | 观察 GC 后存活量，结合类直方图、heap dump 和对象持有关系 | 用 MAT 等工具看支配树、保留大小、到 GC Roots 的路径；检查静态集合、ThreadLocal、缓存、监听器及类加载器 |

Go 的 RSS 上涨而 Go 堆稳定时，继续检查 cgo 分配、线程栈、映射和运行时保留内存。Java 堆稳定时，继续检查 DirectBuffer、线程、Metaspace 和 native 分配；预先开启 NMT 后可做基线差分，但 NMT 不能覆盖所有第三方 native 分配。线上采 heap dump 还需考虑暂停与磁盘开销。见 [Go 诊断文档](https://go.dev/doc/diagnostics)、[Oracle 内存排查指南](https://docs.oracle.com/en/java/javase/25/troubleshoot/troubleshooting-memory-leaks.html)和 [jcmd 影响说明](https://docs.oracle.com/en/java/javase/25/docs/specs/man/jcmd.html)。

## 面试追问时的回答边界

“栈已经坏了怎么定位”可以这样回答：

> 我先核对二进制、符号和故障线程，再从寄存器、故障指令和反汇编确认局部数组与返回地址槽的位置。原始内存若显示覆盖内容与输入连续对应，就结合长度来源和所有写入点缩小范围。最后在新运行中用硬件写监视点或 ASan 捕获非法写入。无法从现有 core 恢复的历史调用链，我会明确说明，并补充采集或复现证据。

记录真实排查结果时分别写明退出原因、现场身份、已证明的破坏范围、待验证的根因、复现证据和修复回归结果；教学示意与项目实测记录分开保存。本篇是跨语言问答归档，没有新增运行时功能，也不构成 C++/Go 版或独立 Java 版的故障修复与验收记录。
