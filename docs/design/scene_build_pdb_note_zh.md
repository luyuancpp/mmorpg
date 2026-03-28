# Scene 构建 PDB 冲突说明

- `cpp/libs/services/scene/scene.vcxproj` 和 `cpp/nodes/scene/scene.vcxproj` 都使用项目名 `scene`；如果不显式指定编译器 `ProgramDataBaseFileName`，并行解决方案构建可能会在 `scene.pdb` 上触发 `C1041` 错误。
- 将 scene 服务库的编译器 PDB 隔离到 `$(IntDir)` 下（`$(IntDir)$(ProjectName).compile.pdb`），以避免 `/m` 并行构建时跨项目 PDB 争用。
