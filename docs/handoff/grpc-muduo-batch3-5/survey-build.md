<!-- 只读调查结果(2026-10-15,基线 HEAD 526780b4e 加当时工作区的未提交改动;第 3 批前半已落地,行号可能已漂移)。原样保留作交接附录,见同目录 README.md。 -->

# AREA build
## summary
现状结论(只读调查,未编译、未运行任何构建/生成命令):官方 gRPC v1.83.0(third_party/grpc 子模块,c876f4da)目前不只是 RPC 运行时,它还是全仓 protobuf 35.1(C++ 7.35.1)、abseil LTS 20250512.1、BoringSSL(ssl/crypto)、zlib(Windows)、googletest/gmock(Windows 测试)、protoc.exe 35.1 与 zlibd.dll 的唯一来源。
- Windows:根 Directory.Build.targets 给所有 vcxproj 注入 grpc/third_party/utf8_range。3 个节点(cpp/nodes/{gate,scene,battle})和 25 个测试工程(合计正好 28 个,与设计 §10 第 5 批一致)链接全套 grpc 库,库目录指向 third_party/grpc/install_vs2026_dbg/lib。16 个静态库工程的头文件路径都来自 grpc 树,其中真正用到 grpc++ 头的只有 grpc_client、proto、core、session、services/scene,外加节点自身;其余只为取 protobuf/abseil 头。
- Linux:三个节点的 CMakeLists 写死了 find_package(absl/protobuf/gRPC) 和 gRPC:: 目标,这些文件由 tools/archived/vcxproj2cmake.py 生成。deploy/k8s/Dockerfile.cpp 与 tools/archived/setup_dependencies.sh 用 gRPC 的 module provider 把 protobuf/abseil/BoringSSL/c-ares/re2 一起装进 /usr/local,没有独立安装的 protobuf/abseil。
- proto 生成:tools/proto_generator/protogen/internal/generator/cpp/gen.go 从 PATH 调 protoc 与 grpc_cpp_plugin,把 *.grpc.pb.{h,cc} 写到 cpp/generated/proto。磁盘上有 30 对,其中 27 对已跟踪,player_inbox 的 3 对未跟踪;proto.vcxproj 与 CMakeLists 编译其中 28 个。注意 common/constants/etcd_grpc.pb.cc 是普通 pb,不能删。
- 改独立依赖的可行路径:把 grpc 子模块里钉住的同一提交拆成独立子模块,即 protobuf 35cd01f(内含 utf8_range)和 abseil 76bb243(20250512.1,正是 protobuf 35.1 的 cmake/dependencies.cmake 指定的版本)。ABI 与现有生成物的 PROTOBUF_VERSION 7035001 校验都不变。
- 第 5 批最大的隐性改动面:OpenSSL/BoringSSL(gate/scene/battle 的 HMAC 全靠它)、gtest/gmock、Windows zlib 与 zlibd.dll、protoc.exe 路径(dev.bat、导表器、go/build.ps1、protogen 的 protobuf_dir 都指向 grpc 树)。
- 关键文件:docs/design/grpc-muduo-transport.md §9/§10;Directory.Build.targets;cpp/nodes/*/[a-z]*.vcxproj;tools/archived/vcxproj2cmake.py;tools/archived/setup_dependencies.sh;deploy/k8s/Dockerfile.cpp;tools/proto_generator/protogen/internal/generator/cpp/gen.go;tools/scripts/third_party/build_grpc.ps1。
- 过程说明:调查中我误用一次 shell 重定向,创建了 C:\Users\luyua\AppData\Local\Temp\x_disk(/tmp/x_disk)。发现后已立即 rm 删除并确认不存在,仓库内没有任何文件改动。
## facts
### 根级 MSBuild 注入
Directory.Build.targets:35-44 给所有 .vcxproj 加 third_party\grpc\third_party\utf8_range\(注释写明 protobuf v35.1 的 parse_context.h 直接 include utf8_validity.h),并全局禁用 C4996(注释说是为 gRPC v1.83 的弃用类型)。:46-51 是工作区未提交的新改动,只对 grpc_client 工程加 muduo windows 兼容头。
files: D:\luyuan\xuanming-server-mmo-cpp\Directory.Build.targets
### 真链接 grpc++ 的 28 个工程(Debug|x64)
链接项包括 address_sorting、cares、crypto、gpr、grpc++、grpc++_alts、grpc++_error_details、grpc++_reflection、grpc++_unsecure、grpc、grpcpp_channelz、grpc_authorization_provider、grpc_plugin_support、grpc_unsecure、libprotobuf-lited、libprotobufd、libprotocd、libupbd、libutf8_range、libutf8_validity、re2、ssl、upb_{base,hash,json,lex,mem,message,mini_descriptor,mini_table,reflection,textformat,wire}_lib、utf8_range_lib、zlibd、zlibstaticd,外加 88 个 absl_*.lib。库目录是 ../../../third_party/grpc/install_vs2026_dbg/lib/。
- 3 个节点:gate.vcxproj L159/L160(Release L216),scene.vcxproj L119/L120(Release L158),battle.vcxproj L140/L141(Release L194)。
- 25 个测试(行号在 L126~L141 之间):aoi_delivery_test、aoi_test、bag_test、buff_test、configuration_table_test、consistent_hash_node_test、cool_down_time_test、cross_zone_test、currency_test、kafka_command_test、message_limiter、missions、server_sequence(node_sequence_test)、player_landing_test、proto_field_checker_test、readfile2string、reward、routing_identity_test、rpc_controller_test、skill_test、time_meter_test、time_util_test、timer_destroy_test、timer_queue_unit_test、turn_battle_engine_test。
- 列表中的 grpc_client.lib 是本仓库自己的库(lib/grpc_client.lib),不是官方 grpc 库,删依赖时不能误删。
files: D:\luyuan\xuanming-server-mmo-cpp\cpp\nodes\gate\gate.vcxproj, D:\luyuan\xuanming-server-mmo-cpp\cpp\nodes\scene\scene.vcxproj, D:\luyuan\xuanming-server-mmo-cpp\cpp\nodes\battle\battle.vcxproj, D:\luyuan\xuanming-server-mmo-cpp\cpp\tests\rpc_controller_test\rpc_controller_test.vcxproj, D:\luyuan\xuanming-server-mmo-cpp\cpp\tests\consistent_hash_node_test\consistent_hash_node_test.vcxproj
### 只为 protobuf/abseil/gtest 引用 grpc 树的工程
- agones_lifecycle_test.vcxproj L124/L140/L141 与 client_endpoint_test.vcxproj L124/L140/L141:只链 libprotobufd、libutf8_range、libutf8_validity 和 88 个 absl,库目录仍是 grpc install;client_endpoint_test 另用 grpc 树里的 googletest 头。
- ecs_context_test.vcxproj:41 引 grpc/third_party/abseil-cpp;:57-58 直接编 grpc/third_party/googletest 的 gtest-all.cc 与 gtest_main.cc。
- snow_flake.vcxproj L124/L151 只引 grpc 的 abseil 头;:168-173 已改为编 yaml-cpp 自带的 googletest-1.16.0。agones_lifecycle_test 与 muduo_grpc_test 用的也是这一套,是现成先例。
- muduo_grpc_test.vcxproj 完全不依赖 grpc,只链 muduo_grpc、nghttp2、muduo。
- mrediscli.vcxproj 与 redis.vcxproj 是遗留工程,引不存在的 third_party/protobuf/src 和 common.lib 等。
files: D:\luyuan\xuanming-server-mmo-cpp\cpp\tests\agones_lifecycle_test\agones_lifecycle_test.vcxproj, D:\luyuan\xuanming-server-mmo-cpp\cpp\tests\client_endpoint_test\client_endpoint_test.vcxproj, D:\luyuan\xuanming-server-mmo-cpp\cpp\tests\ecs_context_test\ecs_context_test.vcxproj, D:\luyuan\xuanming-server-mmo-cpp\cpp\tests\snow_flake_test\snow_flake.vcxproj, D:\luyuan\xuanming-server-mmo-cpp\cpp\tests\muduo_grpc_test\muduo_grpc_test.vcxproj
### 静态库工程的头文件路径(x64)
- 引 third_party/grpc/include、grpc/third_party/(protobuf/src、abseil-cpp)的:
  - grpc_client.vcxproj L169/L196
  - proto.vcxproj L375/L402
  - proto_helpers L131/L158
  - rpc L183/L210
  - core L95/L141(另含 generated/grpc_client 与 openssl/include)
  - session L99/L144
  - modules L142/L187
  - services/battle L107/L132、services/gate L104/L148、services/player L97/L141
  - services/scene L238/L286(另含 grpc/third_party/boringssl-with-bazel/include)
- 只引 protobuf/abseil 的:table L425/L453、config L103/L148、infra L159/L204、thread_context L107/L152、muduo_windows/muduo.vcxproj L194/L238。
- 真正 #include grpcpp 或 *.grpc.pb.h 的只有:grpc_client(生成物)、proto(*.grpc.pb.cc)、core、session、services/scene(player_inbox_system.cpp,另有 5 个文件 include grpc_client 头),以及 nodes/scene、nodes/battle 的 handler。
- modules、services/gate、services/battle、services/player、rpc、proto_helpers 里没查到直接 include grpc 或 grpc_client 头(按 include 名 grep),它们的 grpc/include 路径大概率多余(推测)。
files: D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\core\core.vcxproj, D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\services\scene\scene.vcxproj, D:\luyuan\xuanming-server-mmo-cpp\cpp\generated\proto\proto.vcxproj, D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\session\session.vcxproj
### grpc 安装目录捎带的非 gRPC 产物(Windows)
third_party/grpc/install_vs2026_dbg 目前提供:
- lib/:gtest.lib、gtest_main.lib、gmock.lib、gmock_main.lib(25 个测试都在链)
- lib/:zlibd.lib、zlibstaticd.lib
- lib/:ssl.lib、crypto.lib(BoringSSL)
- bin/:protoc.exe(libprotoc 35.1)、grpc_cpp_plugin.exe、zlibd.dll

版本错位:各工程的 zlib 头取根目录 third_party/zlib(zlib.h:44 是 1.3.2.1-motley),库却是 grpc 自带 zlib 编的(1.3.1.1-motley)。

运行期:run_cpp_tests.ps1:118-122 与 start_game.ps1:587 都从 grpc 的 install bin 取 zlibd.dll。

Release 配置:install_vs2026(Release)本机不存在;节点 Release|x64 的库目录只有 ../../../x64/Release/ 和 ../../../lib/,不含 grpc install。consistent_hash_node_test L127 的 Debug 库目录也只有 ../../../lib/。
files: D:\luyuan\xuanming-server-mmo-cpp\third_party\grpc\install_vs2026_dbg\lib, D:\luyuan\xuanming-server-mmo-cpp\tools\scripts\run_cpp_tests.ps1, D:\luyuan\xuanming-server-mmo-cpp\tools\scripts\start_game.ps1, D:\luyuan\xuanming-server-mmo-cpp\third_party\zlib\zlib.h
### OpenSSL/BoringSSL 依赖
- 使用方:HMAC-SHA256 与 CRYPTO_memcmp 写在 cpp/libs/engine/core/security/token_security.h:18-20/50/73(<openssl/crypto.h|evp.h|hmac.h>),被 gate_security.h:16-18、battle_security.h、scene 的 asset_op_auth.{h,cpp}、player_gm_guard.h、scene_admin_handler.cpp 等引用。
- Windows:头路径是 grpc/third_party/boringssl-with-bazel/include 排在 third_party/openssl/include 之前(三个节点 vcxproj 与 services/scene 均如此),库是 grpc install 的 ssl.lib/crypto.lib。
- Linux:cpp/nodes/scene/CMakeLists.txt:24-58 的注释写明头取系统 OpenSSL 3(libssl-dev),-lssl -lcrypto 却因 link_directories 中 /usr/local/lib 排在前面而取到 gRPC 装进去的 BoringSSL 静态库,是已知的头库错位。
- grpc 构建前要打补丁 tools/patches/grpc/v1.83.0-boringssl-windows-x509-name.patch(build_grpc.ps1:71、Dockerfile.cpp:66,113-120)。
files: D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\core\security\token_security.h, D:\luyuan\xuanming-server-mmo-cpp\cpp\nodes\gate\gate_security.h, D:\luyuan\xuanming-server-mmo-cpp\cpp\nodes\scene\CMakeLists.txt, D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\services\scene\CMakeLists.txt, D:\luyuan\xuanming-server-mmo-cpp\tools\patches\grpc\v1.83.0-boringssl-windows-x509-name.patch
### Linux CMake
- 节点:gate CMakeLists.txt:79-81、scene :118-120、battle :63-65 是 find_package(absl/protobuf/gRPC CONFIG REQUIRED)。链接项 gRPC::grpc++、grpc++_reflection、grpc、gpr 与 protobuf::libprotobuf(gate :109-113、scene :153-158、battle :94-99),外加 ssl、crypto,以及 gate/scene 的 35 个 absl:: 目标。
- 库:core CMakeLists.txt:30 与 session、modules、services/{battle,gate,scene} 都引 third_party/grpc/third_party/;其余库引不存在的 ../third_party/protobuf/src。
- 生成器:这些文件由 tools/archived/vcxproj2cmake.py 生成。
  - SKIP_INCLUDE_PATTERNS :47-56 丢掉 grpc 的 include/protobuf/abseil/re2/utf8_range
  - EXTERNAL_LIBS :61-82 含 gRPC:: 目标、ssl、crypto、z
  - ABSEIL_TARGETS :85-99
  - :307-309 给每个 exe 写 find_package(gRPC)
- 漂移:仓库里已提交的 CMakeLists.txt 没有生成器写的 'GENERATED FILE' 横幅,也缺 GOOGLE_* 宏,与生成器输出不一致;build_linux.sh 不带 --skip-generate 时会重新生成覆盖。
files: D:\luyuan\xuanming-server-mmo-cpp\cpp\nodes\gate\CMakeLists.txt, D:\luyuan\xuanming-server-mmo-cpp\cpp\nodes\scene\CMakeLists.txt, D:\luyuan\xuanming-server-mmo-cpp\cpp\nodes\battle\CMakeLists.txt, D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\core\CMakeLists.txt, D:\luyuan\xuanming-server-mmo-cpp\tools\archived\vcxproj2cmake.py, D:\luyuan\xuanming-server-mmo-cpp\tools\scripts\build_linux.sh
### Linux 依赖构建与 CI
- Dockerfile.cpp:52-135(deps 阶段):git clone grpc v1.83.0 并逐个校验 grpc c876f4da、protobuf 35cd01f9、BoringSSL 2b44a370,打补丁后 cmake install 到 /usr/local,写 stamp /usr/local/share/mmorpg-deps/grpc-linux.stamp,最后删源码。gRPC 的 *_PROVIDER 默认都是 module(third_party/grpc/CMakeLists.txt:74-96),protobuf、abseil、BoringSSL、c-ares、re2 随之一起安装。
- 其他 Dockerfile 要点:apt 装 libssl-dev(:40-48);运行镜像装 libssl3t64,并 COPY /usr/local/lib/*.so*(:213-233)。
- setup_dependencies.sh:94-184 用同一组钉住的提交号和 stamp 编 grpc;zlib 段在后面(:362-390);nghttp2 段 :392-483 是可复用的写法模板。
- .dockerignore:12-14 排除 third_party/grpc/ 与 openssl/。
- cpp-build-ci.yml:146-240 的 compile-nodes 走 Dockerfile 的 builder 阶段,子模块清单(:185-208)不含 grpc;:11-16 注释写明 7035001 硬校验锁定了必须用自编 grpc。include-cleaner-ci.yml 与 release.yml 不涉及 grpc。
files: D:\luyuan\xuanming-server-mmo-cpp\deploy\k8s\Dockerfile.cpp, D:\luyuan\xuanming-server-mmo-cpp\tools\archived\setup_dependencies.sh, D:\luyuan\xuanming-server-mmo-cpp\.dockerignore, D:\luyuan\xuanming-server-mmo-cpp\.github\workflows\cpp-build-ci.yml
### 版本
- gRPC:1.83.0。证据:third_party/grpc/CMakeLists.txt:28、include/grpcpp/version_info.h:25、install 的 gRPCConfigVersion.cmake;子模块 HEAD c876f4da;.gitmodules:50-54 写 branch v1.83.x、ignore=dirty。
- protobuf:35.1(C++ 7.35.1;protobuf/version.json,CMakeLists.txt:93),子模块 35cd01f9。生成的 *.pb.h 硬校验 #if PROTOBUF_VERSION != 7035001(node.pb.h:16)。
- abseil:LTS 20250512 patch 1(config.h:120-121,inline namespace lts_20250512),子模块 76bb2432。protobuf 35.1 自己的 cmake/dependencies.cmake:15 要求的正是 abseil 20250512.1。
- 其他:BoringSSL 2b44a370;googletest 52eb8108;grpc 内 zlib 1.3.1.1;nghttp2 1.70.0(85e300c7)。
- install 目录:third_party/grpc/install_vs2026_dbg(Debug);bin/protoc.exe 输出 libprotoc 35.1。
- grpc 构建配置(.build_ninja_debug/CMakeCache.txt):protobuf_MSVC_STATIC_RUNTIME 与 ABSL_MSVC_STATIC_RUNTIME 均为 OFF(/MDd)。build_grpc.ps1:351-379 用 /std:c++20;grpc 的 cmake/abseil-cpp.cmake:28 强制 ABSL_PROPAGATE_CXX_STD ON。
files: D:\luyuan\xuanming-server-mmo-cpp\.gitmodules, D:\luyuan\xuanming-server-mmo-cpp\third_party\grpc\third_party\protobuf\cmake\dependencies.cmake, D:\luyuan\xuanming-server-mmo-cpp\cpp\generated\proto\common\base\node.pb.h, D:\luyuan\xuanming-server-mmo-cpp\tools\scripts\third_party\build_grpc.ps1
### abseil 头与库的 options 不一致
各工程 include 的是 grpc 源码树里的 abseil-cpp/absl/base/options.h,其中 USE_STD_STRING_VIEW 与 USE_STD_ORDERING 都是 2(按编译期自动判断);库按 install 的 options.h 编,这两项是 1。工程都用 C++17 以上,所以实际结果一致。改独立依赖后应改为 include install 目录的头,避免同类隐患。
files: D:\luyuan\xuanming-server-mmo-cpp\third_party\grpc\third_party\abseil-cpp\absl\base\options.h, D:\luyuan\xuanming-server-mmo-cpp\third_party\grpc\install_vs2026_dbg\include\absl\base\options.h
### proto 生成链
- gen.go:35-53:每个 rpc.type 为 grpc 或 etcd 的域都会额外跑 BuildProtoGrpcCpp。
- gen.go:202-232 的 GenerateCppGrpc 用 exec.LookPath 找 grpc_cpp_plugin,调 protoc --grpc_out 写到 grpc_temp_dir;:234-283 再拷到 grpc_proto_output_dir(cpp/generated/proto/)。
- protoc 统一从 PATH 取(utils.go:56、descriptor.go:155、go/gen.go:147)。dev.bat:523-538 的 :prepare_protoc 把 third_party\grpc\install_vs2026_dbg\bin 放进 PATH,并校验 libprotoc 35.1。
- proto_gen.yaml:10-11 的 grpc_dir、protobuf_dir 指向 third_party/grpc/third_party/protobuf/src/,用于 WKT 的 --proto_path;C++、Go 生成器和 grpc_handler_target.go:57 都在用。no-unity 版配置同样如此。
- 其他 WKT 路径:go/build.ps1:34,44(goctl)、tools/data_table_exporter/exporter_config.yaml:33。
- protoc.exe 路径写死在 sandbox_export.py:48 与 verify_schema_parity.py:40。
files: D:\luyuan\xuanming-server-mmo-cpp\tools\proto_generator\protogen\internal\generator\cpp\gen.go, D:\luyuan\xuanming-server-mmo-cpp\tools\proto_generator\protogen\etc\proto_gen.yaml, D:\luyuan\xuanming-server-mmo-cpp\tools\proto_generator\protogen\etc\proto_gen.no-unity.yaml, D:\luyuan\xuanming-server-mmo-cpp\dev.bat, D:\luyuan\xuanming-server-mmo-cpp\go\build.ps1, D:\luyuan\xuanming-server-mmo-cpp\tools\data_table_exporter\exporter_config.yaml, D:\luyuan\xuanming-server-mmo-cpp\tools\data_table_exporter\tools\sandbox_export.py, D:\luyuan\xuanming-server-mmo-cpp\tools\data_table_exporter\tools\verify_schema_parity.py
### *.grpc.pb.{h,cc} 产物与编译面
- 磁盘上 30 对:27 对已跟踪,player_inbox/{player_inbox,player_inbox_admin,player_inbox_table} 3 对未跟踪;data_service.grpc.pb.* 工作区有未提交修改。
- proto.vcxproj 与 cpp/generated/proto/CMakeLists.txt 各编 28 个 .grpc.pb.cc。friend/friend_table 与 guild/guild_db 的 .grpc.pb.cc 在磁盘上但没被编译。
- 两份清单里都有 common/constants/etcd_grpc.pb.cc,它是 etcd_grpc.proto 的普通 pb,不是 gRPC 桩,删除时不能误伤。
- 这两份清单都是手工维护的,没有生成器写 vcxproj。
files: D:\luyuan\xuanming-server-mmo-cpp\cpp\generated\proto\proto.vcxproj, D:\luyuan\xuanming-server-mmo-cpp\cpp\generated\proto\proto.vcxproj.filters, D:\luyuan\xuanming-server-mmo-cpp\cpp\generated\proto\CMakeLists.txt
### 手写 C++ 对官方 gRPC 的直接依赖
- grpcpp 头:
  - core:node/system/node/node.h:28(node.h:307 持有 grpc::Server;node.cpp:769-844 用 ServerBuilder 建同步服务)
  - core:grpc_channel_cache.h:3-4(:118-143 CreateCustomChannel / ResourceQuota)
  - core:network/grpc_client_runtime.h:9-10(CompletionQueue)、node_utils.h:4(grpc/impl/connectivity_state.h)、node_utils.cpp:10
  - session:system/session.h:5、session.cpp:5
  - nodes:scene/id_segment_bootstrap.cpp:12、scene/handler/grpc/scene_node_service.h:3,5、battle/handler/grpc/battle_node.h:3,5
  - services/scene:player_inbox_system.cpp:11(未跟踪)
  - 生成目录下手工维护的 grpc_client/grpc_call_tag.h:10-11;grpc_init_client.h:7 与 .cpp:9 是生成物
- 直接按路径钻进 grpc 树:core/node/system/node/node_entry.h:7 写的是 #include "grpc/third_party/abseil-cpp/absl/log/initialize.h"。
- 代码直接用 abseil:node_entry.h、type_define.h(absl/hash、absl/numeric/int128),所以 abseil 必须保留为一等依赖。
- 测试用官方 API:tests/rpc_controller_test/grpc_client_lifecycle_test.cpp、tests/cross_zone_test/cross_zone_test.cpp:5-6,1855;routing_identity_test/scene_manager_selector_test.cpp 用了 41 处 GRPC_CHANNEL_*。
- 传输开关仍默认官方实现:etcd_transport.h:84 与 muduo_etcd_transport.cpp:506-519 的 NODE_ETCD_TRANSPORT;gate_router_transport.h:25 的 GATE_ROUTER_TRANSPORT。
files: D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\core\node\system\node\node_entry.h, D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\core\node\system\node\node.h, D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\core\node\system\node\node.cpp, D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\core\node\system\grpc_channel_cache.h, D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\core\network\grpc_client_runtime.h, D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\core\network\node_utils.h, D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\session\system\session.h, D:\luyuan\xuanming-server-mmo-cpp\cpp\nodes\scene\id_segment_bootstrap.cpp, D:\luyuan\xuanming-server-mmo-cpp\cpp\generated\grpc_client\grpc_call_tag.h, D:\luyuan\xuanming-server-mmo-cpp\cpp\tests\routing_identity_test\scene_manager_selector_test.cpp
### 生成器中引用官方 gRPC 的源码与模板
- Go 源码:internal/grpc_handler_gen.go、internal/model.go、internal/config/config.go(GrpcDir、ProtobufDir 字段 :54-55,扩展名 grpc_pb_cc/grpc_pb_h)、generator/cpp/gen.go、generator/cpp/grpc_process.go、grpc_handler_target.go。
- 模板:grpc_async_client.{h,cpp}.tmpl、grpc_init_total.{h,cpp}.tmpl、grpc_service_handler.{h,cpp}.tmpl。
- 相关测试:code_parser_test.go、grpc_handler_gen_test.go、grpc_handler_preservation_test.go、grpc_handler_routing_test.go、grpc_process_test.go。
- 按设计 §10,第 3 批前这些客户端模板由另一会话持有。
files: D:\luyuan\xuanming-server-mmo-cpp\tools\proto_generator\protogen\internal\template\grpc_async_client.cpp.tmpl, D:\luyuan\xuanming-server-mmo-cpp\tools\proto_generator\protogen\internal\template\grpc_service_handler.h.tmpl, D:\luyuan\xuanming-server-mmo-cpp\tools\proto_generator\protogen\internal\grpc_handler_gen.go, D:\luyuan\xuanming-server-mmo-cpp\tools\proto_generator\protogen\internal\config\config.go
### 第三方 googletest 来源
- 约 50 个测试 vcxproj 的头文件路径引 third_party/grpc/third_party/googletest/{googletest,googlemock}/include,链接 grpc install 的 gtest/gmock lib。
- cpp/nodes/gate/tests 与 battle/tests 下 6 个独立测试的注释写着用 GT=third_party/grpc/third_party/googletest/googletest 手工编译。
- 现成替代:yaml-cpp/test/googletest-1.16.0,已被 agones_lifecycle_test、muduo_grpc_test、snow_flake 使用。
files: D:\luyuan\xuanming-server-mmo-cpp\cpp\nodes\gate\tests\gate_security_test.cpp, D:\luyuan\xuanming-server-mmo-cpp\cpp\tests\agones_lifecycle_test\agones_lifecycle_test.vcxproj
### 独立构建 protobuf/abseil 的上游约束
- grpc 子模块里的 protobuf 35.1 没有 .gitmodules;utf8_range 在树内(third_party/protobuf/third_party/utf8_range)。
- abseil 的获取:protobuf 的 cmake/abseil-cpp.cmake:13-36 先 find_package(absl CONFIG),找不到且没设 protobuf_LOCAL_DEPENDENCIES_ONLY 时从 GitHub FetchContent 下载。
- 关键默认值:protobuf CMakeLists.txt:52-57 中 protobuf_BUILD_SHARED_LIBS 与 MSVC_STATIC_RUNTIME 在静态构建时默认 ON(即 /MT,与本仓库的 /MDd 冲突);protobuf_WITH_ZLIB 默认 ON(需要 find_package(ZLIB));:36-38 protobuf_BUILD_PROTOC_BINARIES 与 BUILD_LIBUPB 默认 ON。
- 对比:grpc 当前通过 cmake/protobuf.cmake:32 把 protobuf_MSVC_STATIC_RUNTIME 设为 OFF。
- muduo-linux 的 muduo_protobuf_codec 用 find_package(Protobuf)(muduo-linux/CMakeLists.txt:54)并链 protobuf 与 z。
files: D:\luyuan\xuanming-server-mmo-cpp\third_party\grpc\third_party\protobuf\CMakeLists.txt, D:\luyuan\xuanming-server-mmo-cpp\third_party\grpc\third_party\protobuf\cmake\abseil-cpp.cmake, D:\luyuan\xuanming-server-mmo-cpp\third_party\grpc\cmake\protobuf.cmake, D:\luyuan\xuanming-server-mmo-cpp\third_party\muduo-linux\CMakeLists.txt
### 其他依赖 grpc 路径的脚本与配置
- tools/scripts/third_party/build_grpc.ps1:构建 Release/Debug 两份,装到 install_vs2026 与 install_vs2026_dbg(:477-487),钉住的提交号在 :68-69。
- 其他构建入口:build_grpc_vs2026_v145.bat、tools/archived/build_grpc.ps1、dev_tools.ps1 的 third-party-grpc-build(:539-、:1361、:1454)、.vscode/tasks.json 三个 'third_party: grpc build' 任务。
- check_no_raw_pointer_member.ps1:120-123 用 grpc 的 include/protobuf/abseil/utf8_range 作 -isystem。
- verify_grpc_client_build.ps1 只编我们自己的 grpc_client 工程。
- 文档:tools/scripts/README.md:123-205、deploy/k8s/AGENTS.md:146,154、.github/copilot-instructions.md:102-114(gRPC 同步服务线程池与关机)。
files: D:\luyuan\xuanming-server-mmo-cpp\tools\scripts\dev_tools.ps1, D:\luyuan\xuanming-server-mmo-cpp\.vscode\tasks.json, D:\luyuan\xuanming-server-mmo-cpp\tools\scripts\check_no_raw_pointer_member.ps1, D:\luyuan\xuanming-server-mmo-cpp\tools\scripts\README.md, D:\luyuan\xuanming-server-mmo-cpp\.github\copilot-instructions.md
## changeSurface
- [third-party] D:\luyuan\xuanming-server-mmo-cpp\.gitmodules: 删除 third_party/grpc(:50-54)。新增 third_party/protobuf(钉 35cd01f9)与 third_party/abseil-cpp(钉 76bb2432,即 20250512.1),都设 ignore=dirty,与 nghttp2 同做法;按 SSL 方案可能再加 boringssl 或 googletest
- [build] D:\luyuan\xuanming-server-mmo-cpp\Directory.Build.targets: :41 的 utf8_range 路径改为 protobuf 独立安装目录的 include(或 third_party/protobuf/third_party/utf8_range);:35-43 的 C4996 注释与原因重写;考虑在这里集中定义 protobuf/abseil 的 include 与 lib 目录属性
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\nodes\gate\gate.vcxproj: L159/L216 删除 grpc++、grpc、gpr、upb_*、re2、cares、address_sorting、grpc_plugin_support、libprotocd 等链接项,保留 grpc_client.lib、muduo_grpc.lib、nghttp2.lib;L160 库目录换成新 protobuf/abseil/zlib/ssl 安装目录;L141/L198 去掉 grpc/include 与 boringssl 头路径
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\nodes\scene\scene.vcxproj: L119/L158 链接项、L120 库目录、L99/L138 头路径,改法同 gate
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\nodes\battle\battle.vcxproj: L140/L194 链接项、L141 库目录、L122/L176 头路径,改法同 gate
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\tests\aoi_delivery_test\aoi_delivery_test.vcxproj: L57 去 grpc 链接项,L56 库目录,L44 头路径(含 grpc 树里的 googletest)
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\tests\aoi_test\aoi_test.vcxproj: L126 链接项、L125 库目录、L113/L138 头路径;gtest/gmock 改用新来源
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\tests\bag_test\bag_test.vcxproj: L138 链接项、L137 库目录、L123/L150 头路径、gtest 来源
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\tests\buff_test\buff_test.vcxproj: L138 链接项、L139 库目录、L123/L150 头路径、gtest 来源
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\tests\configuration_table_test\configuration_table_test.vcxproj: L138 链接项、L139 库目录、L123/L150 头路径、gtest 来源
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\tests\consistent_hash_node_test\consistent_hash_node_test.vcxproj: L128 链接项、L118 头路径;L127 库目录本来就缺 grpc install 路径(现状存疑)
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\tests\cool_down_time_test\cool_down_time_test.vcxproj: L138 链接项、L139 库目录、L124/L151 头路径、gtest 来源
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\tests\cross_zone_test\cross_zone_test.vcxproj: L140 链接项、L139 库目录、L124 头路径;配套 cross_zone_test.cpp 要去掉 grpc::CreateChannel
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\tests\currency_test\currency_test.vcxproj: L138 链接项、L137 库目录、L122/L149 头路径、gtest 来源
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\tests\kafka_command_test\kafka_command_test.vcxproj: L138 链接项、L137 库目录、L122/L149 头路径、gtest 来源
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\tests\message_limiter_test\message_limiter.vcxproj: L140 链接项、L139 库目录、L124/L151 头路径、gtest 来源
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\tests\missions_test\missions.vcxproj: L137 链接项、L136 库目录、L121/L148 头路径、gtest 来源
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\tests\node_sequence_test\server_sequence.vcxproj: L139 链接项、L138 库目录、L124/L151 头路径、gtest 来源
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\tests\player_landing_test\player_landing_test.vcxproj: L138 链接项、L137 库目录、L122/L149 头路径、gtest 来源
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\tests\proto_field_checker_test\proto_field_checker_test.vcxproj: L141 链接项、L142 库目录、L127/L154 头路径、gtest 来源
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\tests\readfile2string_test\readfile2string.vcxproj: L140 链接项、L139 库目录、L124/L151 头路径、gtest 来源
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\tests\reward_test\reward.vcxproj: L137 链接项、L136 库目录、L121/L148 头路径、gtest 来源
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\tests\routing_identity_test\routing_identity_test.vcxproj: L138 链接项、L137 库目录、L122/L149 头路径;配套 scene_manager_selector_test.cpp 的 41 处 GRPC_CHANNEL_* 要改为新状态机
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\tests\rpc_controller_test\rpc_controller_test.vcxproj: L126 链接项、L127 库目录、L114/L158 头路径;L102 的遗留 Debug|Win32 配置也带 grpc 库;grpc_client_lifecycle_test.cpp 需要删除或重写
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\tests\skill_test\skill_test.vcxproj: L138 链接项、L139 库目录、L123/L150 头路径、gtest 来源
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\tests\time_meter_test\time_meter_test.vcxproj: L138 链接项、L139 库目录、L124/L151 头路径、gtest 来源
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\tests\time_util_test\time_util_test.vcxproj: L137 链接项、L138 库目录、L125 头路径(含层级写错的 ../../third_party/grpc/...)
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\tests\timer_destroy_test\timer_destroy_test.vcxproj: L130 链接项、L131 库目录、L117/L162 头路径;L105 遗留 Win32 配置
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\tests\timer_queue_unit_test\timer_queue_unit_test.vcxproj: L126 链接项、L127 库目录、L114/L158 头路径;L102 遗留 Win32 配置
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\tests\turn_battle_engine_test\turn_battle_engine_test.vcxproj: L138 链接项、L139 库目录、L123/L150 头路径、gtest 来源
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\tests\agones_lifecycle_test\agones_lifecycle_test.vcxproj: 不在 28 个之列,但 L124/L140/L141 的 protobuf、abseil 头路径与库目录都指向 grpc
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\tests\client_endpoint_test\client_endpoint_test.vcxproj: L124/L140/L141 的 protobuf/abseil 和 googletest 头都来自 grpc 树
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\tests\ecs_context_test\ecs_context_test.vcxproj: :41 abseil 头路径;:57-58 直接编 grpc 树里的 gtest-all.cc 与 gtest_main.cc
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\tests\snow_flake_test\snow_flake.vcxproj: L124/L151 abseil 头路径来自 grpc 树
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\generated\proto\proto.vcxproj: 删除 28 个 *.grpc.pb.cc 与 *.grpc.pb.h 条目(保留 common/constants/etcd_grpc.pb.cc);L375/L402 去掉 grpc/include
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\generated\proto\proto.vcxproj.filters: 同步删掉 grpc.pb 条目(当前 53 处匹配)
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\generated\proto\CMakeLists.txt: 删除 28 个 *.grpc.pb.cc(由 vcxproj2cmake 重新生成,需同步核对)
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\generated\grpc_client\grpc_client.vcxproj: L169/L196 去掉 grpc/include,protobuf/abseil 路径换成新来源
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\core\core.vcxproj: L95/L141 及文件级覆盖(L274/L354)的头路径;移除 grpc_etcd_transport.cpp/.h(:185/:402)、grpc_client_runtime.h(:389)、grpc_channel_cache.h(:407)条目(前提:第 3 批已删掉相应代码);.filters 同步
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\session\session.vcxproj: L99/L144 头路径
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\modules\modules.vcxproj: L142/L187 头路径(没查到直接用 grpc,grpc/include 推测可删)
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\services\scene\scene.vcxproj: L238/L286 头路径,含 boringssl include 与 openssl 的排序
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\services\gate\gate.vcxproj: L104/L148 头路径
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\services\battle\battle.vcxproj: L107/L132 头路径
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\services\player\player.vcxproj: L97/L141 头路径
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\generated\proto_helpers\proto_helpers.vcxproj: L131/L158 头路径
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\generated\rpc\rpc.vcxproj: L183/L210 头路径
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\generated\table\table.vcxproj: L425/L453 protobuf/abseil 头路径
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\config\config.vcxproj: L103/L148 protobuf/abseil 头路径
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\infra\infra.vcxproj: L159/L204 protobuf/abseil 头路径
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\thread_context\thread_context.vcxproj: L107/L152 protobuf/abseil 头路径
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\muduo_windows\muduo.vcxproj: L194/L238 protobuf/abseil 头路径(ProtobufCodecLite 需要)
- [build] D:\luyuan\xuanming-server-mmo-cpp\tools\archived\vcxproj2cmake.py: EXTERNAL_LIBS(:61-82)删掉 gRPC:: 目标;:307-309 删 find_package(gRPC);SKIP_INCLUDE_PATTERNS(:47-56)改为新 protobuf/abseil 路径;同步 :20-22 与 :45-46 的注释
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\nodes\gate\CMakeLists.txt: 已提交版本里的 find_package(gRPC)(:81)、gRPC:: 目标(:109-112)和 boringssl include(:27);生成器会覆盖,但已提交文件与生成器输出有漂移,要一并核对
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\nodes\scene\CMakeLists.txt: :120 与 :153-156 的 gRPC 项;:24-58 的 BoringSSL/OpenSSL 注释与 include 排序要按新 SSL 方案重写
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\nodes\battle\CMakeLists.txt: :65 与 :94-97 的 gRPC 项
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\core\CMakeLists.txt: :30 的 grpc/third_party/ 头路径,:66 的 grpc_etcd_transport.cpp(由生成器重新生成)
- [build] D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\services\scene\CMakeLists.txt: :29-47 的 grpc/third_party 与 boringssl 头路径
- [build] D:\luyuan\xuanming-server-mmo-cpp\tools\archived\setup_dependencies.sh: §2(:94-184)把 grpc 构建换成 abseil 与 protobuf 两段(提交号核对与 stamp 仿照 nghttp2 段);protobuf 需要 ZLIB,所以 zlib 段(:362-390)要提前;同步 :5-6 与 :490 的提示
- [build] D:\luyuan\xuanming-server-mmo-cpp\deploy\k8s\Dockerfile.cpp: 删掉 :52-135 的 grpc 克隆、BoringSSL 补丁与构建层,改为 COPY 或钉提交克隆 abseil/protobuf 并 install 到 /usr/local;去掉 :66 的补丁 COPY;:13-16 与 :173-174 的注释;保留 libssl-dev 与 libssl3t64
- [build] D:\luyuan\xuanming-server-mmo-cpp\.dockerignore: :12 的 third_party/grpc/ 条目改为排除新子模块的 Windows install 目录(仿照 nghttp2 :18-20)
- [build] D:\luyuan\xuanming-server-mmo-cpp\.github\workflows\cpp-build-ci.yml: :185-208 的子模块清单加入 protobuf 与 abseil-cpp;:11-16、:154、:170-176 的 gRPC 注释
- [build] D:\luyuan\xuanming-server-mmo-cpp\tools\scripts\build_linux.sh: :233-235 的注释(deps 里有 gRPC);工程清单不变
- [generator-source] D:\luyuan\xuanming-server-mmo-cpp\tools\proto_generator\protogen\internal\generator\cpp\gen.go: 删除 BuildProtoGrpcCpp、GenerateCppGrpc、copyCppGrpcOutputs(:162-283)以及 :35-53 的调度分支,停用 grpc_cpp_plugin
- [generator-source] D:\luyuan\xuanming-server-mmo-cpp\tools\proto_generator\protogen\internal\config\config.go: GrpcDir、ProtobufDir、GrpcTempDir、GrpcProtoOutputDir 及 GrpcPbCc/GrpcPbH 扩展名字段的取舍
- [generator-source] D:\luyuan\xuanming-server-mmo-cpp\tools\proto_generator\protogen\etc\proto_gen.yaml: :10-11 的 grpc_dir 与 protobuf_dir 改指 third_party/protobuf/src/;去掉 grpc_temp_dir、grpc_proto_output_dir 及 grpc_pb_cc/h(:38,:46,:104-105)
- [generator-source] D:\luyuan\xuanming-server-mmo-cpp\tools\proto_generator\protogen\etc\proto_gen.no-unity.yaml: 与 proto_gen.yaml 同步
- [generator-source] D:\luyuan\xuanming-server-mmo-cpp\tools\proto_generator\protogen\internal\grpc_handler_gen.go: scene/battle 服务端 handler 骨架里的 grpc::Status 与 grpc::ServerContext(第 4 批改完后,第 5 批核对已不剩 grpc 类型)
- [template] D:\luyuan\xuanming-server-mmo-cpp\tools\proto_generator\protogen\internal\template\grpc_service_handler.h.tmpl: 服务端 handler 模板中的 grpcpp 引用(第 4 批)
- [template] D:\luyuan\xuanming-server-mmo-cpp\tools\proto_generator\protogen\internal\template\grpc_service_handler.cpp.tmpl: 同上
- [template] D:\luyuan\xuanming-server-mmo-cpp\tools\proto_generator\protogen\internal\template\grpc_async_client.h.tmpl: 客户端模板改为与传输无关、不 include *.grpc.pb.h(第 3 批,第 5 批核对)
- [template] D:\luyuan\xuanming-server-mmo-cpp\tools\proto_generator\protogen\internal\template\grpc_async_client.cpp.tmpl: 同上
- [template] D:\luyuan\xuanming-server-mmo-cpp\tools\proto_generator\protogen\internal\template\grpc_init_total.h.tmpl: grpc_init_client 生成物 include grpcpp/grpcpp.h
- [template] D:\luyuan\xuanming-server-mmo-cpp\tools\proto_generator\protogen\internal\template\grpc_init_total.cpp.tmpl: 同上
- [generator-source] D:\luyuan\xuanming-server-mmo-cpp\tools\proto_generator\protogen\internal\generator\cpp\grpc_process.go: 客户端生成流程(第 3 批由另一会话改,第 5 批核对)
- [generator-source] D:\luyuan\xuanming-server-mmo-cpp\tools\proto_generator\protogen\internal\generator\cpp\grpc_handler_target.go: :57 用 ProtobufDir 作 include 路径,随配置改
- [generated-output] D:\luyuan\xuanming-server-mmo-cpp\cpp\generated\proto: 删除全部 *.grpc.pb.{h,cc}(磁盘上 30 对,含 player_inbox 3 对未跟踪),由停用插件后的 proto-gen 重新生成来确认
- [generated-output] D:\luyuan\xuanming-server-mmo-cpp\cpp\generated\grpc_client: 各 *_grpc_client.h 第 9 行 include *.grpc.pb.h,grpc_init_client.{h,cpp} include grpcpp,需由新模板重新生成
- [handwritten] D:\luyuan\xuanming-server-mmo-cpp\cpp\generated\grpc_client\grpc_call_tag.h: 手工维护(:13-14 有注释说明)::10-11 的 grpcpp/client_context.h 与 support/status.h 要去掉,GrpcCallOutcome.code 的 'grpc::StatusCode 同值' 语义改用自有枚举
- [handwritten] D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\core\node\system\node\node_entry.h: :7 按路径 #include grpc 树里的 abseil 头,必须改成 <absl/log/initialize.h>
- [handwritten] D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\core\node\system\node\node.h: :28 include grpcpp,:307 持有 grpc::Server(第 4 批)
- [handwritten] D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\core\node\system\node\node.cpp: :769-844 的 ServerBuilder 同步服务与关机线程(第 4 批)
- [handwritten] D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\core\node\system\grpc_channel_cache.h: 官方通道缓存(CreateCustomChannel、ResourceQuota),删除
- [handwritten] D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\core\network\grpc_client_runtime.h: CompletionQueue 运行时,删除
- [handwritten] D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\core\network\node_utils.h: :4 的 grpc/impl/connectivity_state.h;GetState 选点改用新状态机(第 3 批)
- [handwritten] D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\core\network\node_utils.cpp: :10 grpcpp/channel.h,7 处 GRPC_CHANNEL_*
- [handwritten] D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\core\node\system\etcd\grpc_etcd_transport.cpp: 旧 CQ 版 etcd 传输;删除它与对应 .h
- [handwritten] D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\core\node\system\etcd\muduo_etcd_transport.cpp: :506-519 NODE_ETCD_TRANSPORT 默认值与回退 grpc 分支要删;etcd_transport.h:84 注释同步
- [handwritten] D:\luyuan\xuanming-server-mmo-cpp\cpp\nodes\gate\gate_router_transport.h: GATE_ROUTER_TRANSPORT 默认官方实现,删掉开关或改默认;gate_security_test.cpp:211-280 的断言同步
- [handwritten] D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\engine\session\system\session.h: :5 grpcpp/client_context.h;session.cpp:5 grpcpp.h
- [handwritten] D:\luyuan\xuanming-server-mmo-cpp\cpp\nodes\scene\id_segment_bootstrap.cpp: :12 grpcpp.h(同步阻塞调用或通道状态,需改为 muduo_grpc)
- [handwritten] D:\luyuan\xuanming-server-mmo-cpp\cpp\libs\services\scene\player\system\player_inbox_system.cpp: :11 grpcpp.h 与 GRPC_CHANNEL_*(未跟踪的新文件,归另一会话)
- [handwritten] D:\luyuan\xuanming-server-mmo-cpp\cpp\nodes\scene\handler\grpc\scene_node_service.h: 生成骨架加守护段;:3,5 include grpcpp 与 scene_node_service.grpc.pb.h(第 4 批改模板后重新生成)
- [handwritten] D:\luyuan\xuanming-server-mmo-cpp\cpp\nodes\battle\handler\grpc\battle_node.h: 同上,battle_node.grpc.pb.h
- [handwritten] D:\luyuan\xuanming-server-mmo-cpp\cpp\tests\rpc_controller_test\grpc_client_lifecycle_test.cpp: 官方 grpc 客户端生命周期测试:删除或改测 muduo 版
- [handwritten] D:\luyuan\xuanming-server-mmo-cpp\cpp\tests\cross_zone_test\cross_zone_test.cpp: :5-6 与 :1855-1856 用 grpc::CreateChannel 造 Channel 组件
- [handwritten] D:\luyuan\xuanming-server-mmo-cpp\cpp\tests\routing_identity_test\scene_manager_selector_test.cpp: 41 处 GRPC_CHANNEL_* 状态
- [build] D:\luyuan\xuanming-server-mmo-cpp\tools\scripts\third_party\build_grpc.ps1: 换成 build_abseil 与 build_protobuf 两个脚本(或合为一个,产出 install_vs2026_dbg 与 protoc.exe 35.1;/MDd、protobuf_MSVC_STATIC_RUNTIME=OFF、LOCAL_DEPENDENCIES_ONLY),按需再加 zlib 与 SSL 构建
- [build] D:\luyuan\xuanming-server-mmo-cpp\tools\scripts\third_party\build_grpc_vs2026_v145.bat: 删除
- [build] D:\luyuan\xuanming-server-mmo-cpp\tools\archived\build_grpc.ps1: 删除
- [build] D:\luyuan\xuanming-server-mmo-cpp\tools\scripts\dev_tools.ps1: third-party-grpc-build 命令(:539-、:1361、:1454)换成新依赖的构建命令
- [build] D:\luyuan\xuanming-server-mmo-cpp\.vscode\tasks.json: :123-160 三个 grpc build 任务
- [build] D:\luyuan\xuanming-server-mmo-cpp\dev.bat: :523-538 的 REPO_PROTOC_DIR 改为新 protobuf 安装目录的 bin
- [build] D:\luyuan\xuanming-server-mmo-cpp\tools\scripts\run_cpp_tests.ps1: :122 zlibd.dll 的来源目录
- [build] D:\luyuan\xuanming-server-mmo-cpp\tools\scripts\start_game.ps1: :587 把 grpc install bin 加进 PATH(为了 zlibd.dll)
- [build] D:\luyuan\xuanming-server-mmo-cpp\tools\scripts\check_no_raw_pointer_member.ps1: :120-123 的 -isystem 用 grpc 的 include/protobuf/abseil/utf8_range(每次 PreBuild 都跑,路径失效会影响所有工程的预构建检查)
- [build] D:\luyuan\xuanming-server-mmo-cpp\tools\data_table_exporter\exporter_config.yaml: :33 extra_includes 指向 grpc 树的 protobuf/src
- [build] D:\luyuan\xuanming-server-mmo-cpp\tools\data_table_exporter\tools\sandbox_export.py: :48 写死 grpc install 里的 protoc.exe
- [build] D:\luyuan\xuanming-server-mmo-cpp\tools\data_table_exporter\tools\verify_schema_parity.py: :40 写死 grpc install 里的 protoc.exe
- [build] D:\luyuan\xuanming-server-mmo-cpp\go\build.ps1: :34,:44 goctl 的 --proto_path 指向 grpc 树的 protobuf/src(Go 侧仍用 gRPC,只是 WKT 路径要换)
- [third-party] D:\luyuan\xuanming-server-mmo-cpp\tools\patches\grpc\v1.83.0-boringssl-windows-x509-name.patch: 只服务于 grpc 构建;若改用独立 BoringSSL 子模块则迁移,否则删除
- [doc] D:\luyuan\xuanming-server-mmo-cpp\docs\design\grpc-muduo-transport.md: §9 构建与 §10 第 5 批:写落地记录、protobuf/abseil/SSL/gtest/zlib 的新来源与验证口径
- [doc] D:\luyuan\xuanming-server-mmo-cpp\tools\scripts\README.md: :123-205 的 build_grpc 说明
- [doc] D:\luyuan\xuanming-server-mmo-cpp\deploy\k8s\AGENTS.md: :146、:154 写的是 deps 阶段编 gRPC v1.83.0
- [doc] D:\luyuan\xuanming-server-mmo-cpp\.github\copilot-instructions.md: :102-114 的 gRPC 同步服务线程池与关机规则(第 4、5 批后失效)
- [doc] D:\luyuan\xuanming-server-mmo-cpp\docs\PROGRESS.md: 按 AGENTS §5 追加第 5 批条目;Java 版不适用,设计 §12 已写明
## risks
- SSL 是删除 grpc 时最容易漏掉的硬依赖。gate、scene、battle 的 HMAC-SHA256 与 CRYPTO_memcmp(token_security.h)在 Windows 上靠 grpc install 的 BoringSSL ssl.lib/crypto.lib 与 grpc 树里的 boringssl 头,删掉 grpc 就链接失败。third_party/openssl 是未 configure 的源码,在 Windows 上编需要 Perl 与 NASM。Linux 上去掉 BoringSSL 后,-lssl -lcrypto 会落到系统 OpenSSL 3(反而修正了 scene CMakeLists 注释记录的头库错位);但持久化 Linux 开发机的 /usr/local/lib 里残留的 BoringSSL libssl.a 仍会因为 link_directories 顺序被优先选中,需要清理步骤或 stamp 校验。
- protobuf 版本不能顺手升级。生成的 *.pb.h 硬校验 PROTOBUF_VERSION == 7035001;换版本要用新 protoc 重生成全部 C++ pb,而 C#/Go/Java 是否同批重生,取决于客户端契约,两版都受影响。独立的 protoc.exe 必须仍是 libprotoc 35.1,dev.bat、导表器、protogen 都在校验或依赖它。
- MSVC 运行库与 ABI。protobuf 独立静态构建时,protobuf_MSVC_STATIC_RUNTIME 默认 ON(/MT),而 grpc 构建显式设了 OFF;不显式设 OFF 会出 LNK2038 运行库不匹配。abseil 必须保持 lts_20250512 inline namespace,并统一 ABSL_PROPAGATE_CXX_STD 与 C++ 标准。现状是头取源码树的 options.h(值为 2)、库按 install 的 options.h(值为 1)编,建议迁移时把 include 统一指向 install 目录。
- protobuf CMake 找不到 absl 时会从 GitHub FetchContent 下载。Docker 与 CI 网络不稳(Dockerfile 注释里有记录),应先装 abseil,再以 protobuf_LOCAL_DEPENDENCIES_ONLY=ON 配合 CMAKE_PREFIX_PATH 构建,杜绝静默下载到别的版本。protobuf_WITH_ZLIB 默认 ON,需要先有 ZLIB:setup_dependencies.sh 的 zlib 段要提到 protobuf 之前,Windows 也要独立的 zlib 构建(目前没有脚本)。
- Windows zlib 现在是头取根目录 third_party/zlib(1.3.2.1)、库取 grpc 自带 zlib(1.3.1.1)的混用,运行期还要 grpc install 里的 zlibd.dll(run_cpp_tests.ps1、start_game.ps1、多份文档)。换来源后 DLL 名或静态/动态方式变化,会出现 0xC0000135 静默退出。
- googletest 来源:约 25 个测试链接 grpc install 的 gtest/gmock lib,ecs_context_test 直接编 grpc 树里的 gtest 源码,gate/battle 下独立测试的手工编译命令也指向 grpc 树。删除 grpc 子模块会让这些测试全部失效,需要先定 gtest 来源。
- 删 *.grpc.pb.* 时 proto.vcxproj 与 CMakeLists 里的 common/constants/etcd_grpc.pb.cc 文件名带 _grpc,但它是普通 pb,不能删。friend_table 与 guild_db 的 .grpc.pb.cc 在磁盘上却没被编译,也要清掉。
- 28 个工程各自复制一份约 130 项的链接列表,逐个手改容易漏改或改错。可考虑抽一个共享的 .props 承载 protobuf/abseil 的库目录与链接项,但这是新增结构,需要按 AGENTS §11 说明理由。consistent_hash_node_test 的库目录本来就缺 grpc install,Release|x64 配置本来就指向 x64/Release 且本机没有 install_vs2026,这两处现状本身不完整。
- Linux CMakeLists 由 vcxproj2cmake.py 每次重新生成,已提交的版本已和生成器输出漂移(无 GENERATED 横幅,缺 GOOGLE_* 宏,battle 缺 absl 目标)。只改已提交的 CMakeLists 不改生成器,下次 Linux 构建就会被覆盖回去。
- 第 5 批依赖第 3、4 批全部完成,包括 grpc_call_tag.h、grpc_client 模板、node.cpp 服务端、scene/battle handler 模板、GRPC_CHANNEL_* 选点、两个传输开关的官方回退分支。只要还剩一个 grpcpp 引用,去掉链接项就会编译失败。客户端模板、grpc_process.go 与 grpc_call_tag.h 目前由另一会话持有(设计 §10 末段)。
- 工作区有大量其他会话未提交的改动(git status 中 grpc_client、data_service.grpc.pb.*、Directory.Build.targets、player_inbox 未跟踪文件等)。第 5 批动手前要先分清归属,避免覆盖。
- muduo-linux 用 find_package(Protobuf) 模块模式(muduo-linux/CMakeLists.txt:54)。现在能找到 grpc 装的 protobuf;独立安装到 /usr/local 后理论上照样可用(推测),仍需实测。
- 本次调查过程中误用一次重定向,创建了 %TEMP%\x_disk,已立即删除并确认不存在;仓库内无任何改动。
## openQuestions
- Windows 的 SSL 选哪条路:编 third_party/openssl(需要 Perl 与 NASM),新增独立 BoringSSL 子模块(沿用钉住的 2b44a370 和现有 X509_NAME 补丁),还是用 core 已有的 utils/encode/sha256.cpp 自实现 HMAC 并去掉 OpenSSL 依赖?Linux 是否直接改用系统 OpenSSL 3?
- googletest 来源选哪个:新增 third_party/googletest 子模块(钉 grpc 用的 52eb8108),统一改用 yaml-cpp 自带的 googletest-1.16.0(已有 3 个工程这么用),还是随 protobuf/abseil 一起编 gtest.lib?
- protobuf/abseil 的 Windows 安装目录是否沿用 '子模块内 install_vs2026_dbg 加 ignore=dirty' 的做法(与 grpc、nghttp2 一致)?是否需要 Release 安装?目前 Release|x64 配置并不可用。
- protoc.exe 35.1 由新脚本从源码编(protobuf_BUILD_PROTOC_BINARIES=ON),还是下载官方 protoc-35.1-win64.zip?下载需要用户授权,且要核对来源。
- 是否保留 libprotoc 与 libupb 的链接(运行期大概率用不到,推测)?是否继续需要 libprotobuf-lite?
- Debug|Win32 遗留配置(rpc_controller_test、timer_destroy_test、timer_queue_unit_test、gate/scene 节点)与遗留工程 mrediscli、redis_test 是顺手删除,还是不动?
- third_party/grpc 子模块是彻底移除,还是在过渡期保留给 Go 或其他工具?目前只有 WKT .proto 路径(proto_gen.yaml、go/build.ps1、exporter_config.yaml)用到它,可全部改指 third_party/protobuf/src。
- Windows 是否需要独立的 zlib 构建脚本(现在只有 Linux 的 setup_dependencies.sh 在编 third_party/zlib),以及是否借机消除头文件与库的 zlib 版本错位?

