# VS2026 Cross-Machine Compatibility Maintenance Record

## Fixed Compatibility Issues (2026-03-23)

### Toolset Version Unification
All C++ projects now use `PlatformToolset v145` (VS2026):

**Fixed projects:**
- `cpp/tests/zone_and_area_test/scene_test.vcxproj`: v141 → v145
- `cpp/tests/time_util_test/time_util_test.vcxproj`: v143 → v145
- `cpp/tests/team_test/team.vcxproj`: v143 → v145

### Windows SDK Version Unification
All projects now use `WindowsTargetPlatformVersion 10.0` (automatically uses latest available SDK):

**Fixed projects:**
- `cpp/tests/zone_and_area_test/scene_test.vcxproj`: 10.0.16299.0 → 10.0

### C++ Standard Version Unification
All core projects use `stdcpplatest`:

**Core nodes and services:**
- ✅ scene, gate, centre nodes and service libraries
- ✅ modules libraries
- ✅ All test projects

**Third-party libraries retained:**
- BoringSSL: stdcpp17 (upstream requirement)
- Other gRPC dependencies: mixed stdcpp17/stdcpplatest

## Cross-Machine Deployment Requirements

### VS2026 Installation Requirements
- Visual Studio 2026 (any edition: Community/Professional/Enterprise)
- Windows 10 SDK (latest version, tools auto-select)
- MSVC v145 toolset (included by default with VS2026)

### Verification Commands
```powershell
# Build verification
msbuild game.sln /m /p:Configuration=Debug /p:Platform=x64

# Check toolset consistency
findstr /s /i "PlatformToolset" cpp\**\*.vcxproj | findstr -v "v145"
# Should only show third-party libraries or expected non-v145 projects
```

## Resolved Issue Types
- ❌ Toolset version mismatch causing linker errors
- ❌ Windows SDK version dependency causing build failures
- ❌ C++ standard mixing causing ABI incompatibility
- ❌ Inconsistent cross-machine build results

## Notes
- Third-party libraries (gRPC/BoringSSL) still use mixed standards, but VS2026 handles compatibility well
- If encountering linker issues, check symbol conflicts first rather than standard version issues
