# qdao FairyGUI Project

Open `qdao.fairy` in FairyGUI Editor (the portable copy lives at
`tools/cache/fairygui-editor/FairyGUI-Editor/FairyGUI-Editor.exe`).

The package source lives in `assets/qdao/`. Art comes pre-cut from
`image/ui_controls_v2/` and is wired into the FairyGUI package by
`image/wire_qdao_package.py`. Edit the components in the Editor; Unity
side just binds named children at runtime.

## Layout

| Component | Purpose | Size |
|---|---|---|
| `LoginScreen` | account / password / announcements | 2560×1080 |
| `ServerSelectScreen` | tab filter + server cards | 2560×1080 |
| `SceneScreen` | same as ServerSelectScreen, hud-style entry | 2560×1080 |
| `common/QdaoButtonPrimary` | red lacquer CTA, bg+fg double layer | 480×128 |
| `common/QdaoButtonSecondary` | jade outline, bg+fg double layer | 420×112 |
| `common/QdaoSearchBox` | input field + magnifier icon | 420×76 |
| `common/QdaoTabIdle` | left-column filter pill, idle/active swap via `checked` controller | 360×88 |
| `common/QdaoTabActive` | static active-state shortcut | 360×88 |
| `common/QdaoServerCard` | server row with badge + status traffic-light + title + subtitle + right label | 640×128 |
| `common/QdaoBottomBar` | bottom panel container | 1120×148 |
| `common/QdaoMainPanel` | central jade panel (header + body) | 1280×760 |

## Reimport new art

```powershell
python image/wire_qdao_package.py
```

This:
- copies every PNG from `image/ui_controls_v2/{backgrounds,foregrounds,icons}`
  and `image/q_daoist_scene_bg_2560x1080.png` into `assets/qdao/`
- generates `package.xml` and one `<image>.png.xml` meta per asset, with
  9-grid params for the stretchable templates

Stable hex ids (`0x10000+`) keep references valid across reimports.

## Publish to Unity

```text
client/unity/Assets/Resources/UI/qdao/qdao_fui.bytes
client/unity/Assets/Resources/UI/qdao/qdao_atlas0.png
```

### Manual (free edition)

1. Open `qdao.fairy` in FairyGUI Editor 6.1.4
2. Press **F8** (or menu → Publish)
3. Output path is preconfigured in `settings/Publish.json` to
   `../../unity/Assets/Resources/UI/qdao`

### Command-line (Pro edition only)

```powershell
pwsh -File client/fairygui/qdao/publish_qdao.ps1
```

Default `-EditorPath` points at the portable copy. The script will exit
non-zero on the free edition because `-b qdao` requires a Pro license.

## Unity loading

`AppBootstrap.Awake` calls `UIPackage.AddPackage("UI/qdao/qdao")`. Each
screen (`LoginScreen.cs` / `ServerSelectScreen.cs` / `SceneScreen.cs` /
`RoleCreateScreen.cs` / `HudScreen.cs`) tries
`Theme.TryCreateFromPackage(<componentName>)` first and falls back to a
grey "awaiting new art" placeholder if the package didn't load. Once
the bytes file exists in `Resources/UI/qdao/`, every screen picks up the
new art automatically — no C# code changes required for layout work.

When the new screens are ready for live data, point those screens at
the gateway again (zone fetch, list rendering, button events) — see the
older commits before `chore(ui): wipe qdao screens to placeholders` for
the previous data-binding pattern.
