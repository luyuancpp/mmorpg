# VS2026 跨电脑兼容性维护记录

## 已修复的兼容性问题 (2026-03-23)

### 工具集版本统一
所有C++项目现已统一使用 `PlatformToolset v145` (VS2026)：

**修复的项目：**
- `cpp/tests/zone_and_area_test/scene_test.vcxproj`: v141 → v145
- `cpp/tests/time_util_test/time_util_test.vcxproj`: v143 → v145
- `cpp/tests/team_test/team.vcxproj`: v143 → v145

### Windows SDK版本统一
所有项目现使用 `WindowsTargetPlatformVersion 10.0` (自动使用最新可用SDK)：

**修复的项目：**
- `cpp/tests/zone_and_area_test/scene_test.vcxproj`: 10.0.16299.0 → 10.0

### C++标准版本统一
所有核心项目使用 `stdcpplatest`：

**核心节点和服务：**
- ✅ scene, gate, centre 节点和服务库
- ✅ modules 库
- ✅ 所有测试项目

**第三方库保持：**
- BoringSSL: stdcpp17 (上游要求)
- 其他gRPC依赖: 混合 stdcpp17/stdcpplatest

## 跨电脑部署要求

### VS2026安装要求
- Visual Studio 2026 (任何版本：Community/Professional/Enterprise)
- Windows 10 SDK (最新版本，工具会自动选择)
- MSVC v145 工具集 (VS2026默认包含)

### 验证命令
```powershell
# 编译验证
msbuild game.sln /m /p:Configuration=Debug /p:Platform=x64

# 检查工具集一致性
findstr /s /i "PlatformToolset" cpp\**\*.vcxproj | findstr -v "v145"
# 应该只显示第三方库或预期的非v145项目
```

## 已解决的问题类型
- ❌ 工具集版本不匹配导致的链接错误
- ❌ Windows SDK版本依赖导致的编译失败
- ❌ C++标准混合导致的ABI不兼容
- ❌ 跨机器构建结果不一致

## 注意事项
- 第三方库(gRPC/BoringSSL)仍有混合标准，但VS2026兼容处理较好
- 如遇到链接问题，优先检查符号冲突而非标准版本问题
