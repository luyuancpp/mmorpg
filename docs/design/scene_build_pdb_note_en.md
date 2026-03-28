# Scene Build PDB Note

- `cpp/libs/services/scene/scene.vcxproj` and `cpp/nodes/scene/scene.vcxproj` both use project name `scene`; without an explicit compiler `ProgramDataBaseFileName`, parallel solution builds can hit `C1041` on `scene.pdb`.
- Keep the scene service library compiler PDB isolated under `$(IntDir)` (`$(IntDir)$(ProjectName).compile.pdb`) to avoid cross-project PDB contention during `/m` builds.
