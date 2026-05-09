# qdao FairyGUI Project

Open this folder or `qdao.fairy` with FairyGUI Editor.

The package assets live in `assets/qdao`. The source package already contains the qdao art, reusable slices, `LoginScreen`, `ServerSelectScreen`, and the shared button components. Use FairyGUI Editor as the source of truth for layout. Unity C# only binds named children and updates dynamic values.

Unity runtime load path:

```text
UI/qdao/qdao
```

Expected Unity publish output:

```text
client/unity/Assets/Resources/UI/qdao/qdao_fui.bytes
client/unity/Assets/Resources/UI/qdao/qdao_atlas0.png
```

## Publish Workflow

1. Open `qdao.fairy` in FairyGUI Editor 6.1.4 or newer.
2. Inspect `LoginScreen` and `ServerSelectScreen` under package `qdao`.
3. Set publish path to `client/unity/Assets/Resources/UI/qdao`.
4. Publish package `qdao` as binary format with `.bytes` extension.
5. Confirm `qdao_fui.bytes` and atlas resources exist in the Unity Resources folder.

Do not add the full 2560x1080 mockup PNG to the qdao package. The production screens are built from reusable slices plus runtime-populated lists, so the atlas should not contain a baked full-screen screenshot.

FairyGUI Editor command-line publish is intentionally not the default workflow here. In Editor 6.1.4, `FairyGUI-Editor -p ... -b qdao` exits with `This feature is only avaliable in professional FairyGUI-Editor` on the free edition. The helper script below is kept only for machines with a professional license:

```powershell
pwsh -File client/fairygui/qdao/publish_qdao.ps1 -EditorPath "C:/Path/To/FairyGUI-Editor.exe"
```

Until the binary package is published, Unity falls back to the code-built qdao screen so development stays playable.