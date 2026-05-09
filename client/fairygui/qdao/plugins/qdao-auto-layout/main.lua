local FairyEditor = CS.FairyEditor
local XML = CS.FairyGUI.Utils.XML

local DESIGN_W = 2560
local DESIGN_H = 1080

local function getQdaoPackage()
    local project = FairyEditor.App.project
    if project == nil then
        FairyEditor.App.Alert("Open the qdao FairyGUI project first.")
        return nil
    end

    local pkg = project:GetPackageByName("qdao")
    if pkg == nil then
        pkg = project:CreatePackage("qdao")
    end
    pkg:Open()
    return pkg
end

local function createOrGetComponent(pkg, name)
    local item = pkg:FindItemByName(name)
    if item == nil then
        item = pkg:CreateComponentItem(name, DESIGN_W, DESIGN_H, "", "", true, false)
    end
    return item
end

local function childXml(t, name, x, y, w, h, extra)
    extra = extra or ""
    return XML.New(string.format('<%s name="%s" xy="%d,%d" size="%d,%d" %s/>', t, name, x, y, w, h, extra))
end

local function addChild(component, t, name, x, y, w, h, extra)
    local ok, child = pcall(function()
        return component:CreateChild(childXml(t, name, x, y, w, h, extra))
    end)
    if ok and child ~= nil then
        child.name = name
        child.x = x
        child.y = y
        child.width = w
        child.height = h
    end
    return child
end

local function openComponent(item)
    local doc = FairyEditor.App.docView:OpenDocument(item:GetURL(), true)
    return doc
end

local function prepareComponent(item, note)
    local doc = openComponent(item)
    if doc == nil then
        return nil
    end
    local component = doc.content
    if component == nil then
        component = doc.root
    end
    if component ~= nil then
        component.width = DESIGN_W
        component.height = DESIGN_H
        component.baseNotes = note
        -- designImage intentionally not set: the mockup PNG is loaded by
        -- AppBootstrap (Resources.Load), NOT included in the FairyGUI atlas.
        -- Authoring guides should reference image/ directly instead.
    end
    return component, doc
end

local function buildLogin(pkg)
    local item = createOrGetComponent(pkg, "LoginScreen")
    local component, doc = prepareComponent(item, "Q-daoist login screen. C# binds named children only; visual layout stays in FairyGUI.")
    if component == nil then return end

    addChild(component, "image", "imgSceneBackdrop", 0, 0, DESIGN_W, DESIGN_H, 'src="a000000k" fileName="qdao_scene_backdrop.png"')
    addChild(component, "image", "imgBanner", 1055, 20, 655, 240, 'src="a0000006" fileName="qdao_login_banner.png"')
    addChild(component, "image", "imgRole", 1810, 18, 335, 332, 'src="a000000i" fileName="qdao_role_wanderer.png"')
    addChild(component, "image", "imgBottomBar", 985, 848, 930, 126, 'src="a000000d" fileName="qdao_bottom_bar.png"')
    -- Static art is published by FairyGUI. C# binds the named widgets below
    -- for interaction and dynamic data.
    addChild(component, "inputtext", "inputGateway", 1665, 286, 490, 58, 'prompt="http://127.0.0.1:8888"')
    addChild(component, "inputtext", "inputAccount", 1665, 384, 490, 58, 'prompt="account"')
    addChild(component, "inputtext", "inputPassword", 1665, 482, 490, 58, 'prompt="password" password="true"')
    addChild(component, "component", "listAnnouncement", 0, 0, DESIGN_W, DESIGN_H, '')
    addChild(component, "text", "txtStatus", 1480, 838, 720, 40, 'text=""')
    addChild(component, "component", "btnRefresh", 1640, 885, 210, 76, '')
    addChild(component, "component", "btnEnter", 1885, 872, 300, 92, '')
    if doc ~= nil then doc:Save() end
end

local function buildServerSelect(pkg)
    local item = createOrGetComponent(pkg, "ServerSelectScreen")
    local component, doc = prepareComponent(item, "Q-daoist server-select screen. Server rows are populated by C# into named containers.")
    if component == nil then return end

    addChild(component, "image", "imgSceneBackdrop", 0, 0, DESIGN_W, DESIGN_H, 'src="a000000k" fileName="qdao_scene_backdrop.png"')
    addChild(component, "image", "imgPanelTop", 610, 112, 1420, 173, 'src="a0000007" fileName="qdao_server_scroll.png"')
    addChild(component, "image", "imgSearch", 646, 306, 238, 55, 'src="a000000b" fileName="qdao_search_box.png"')
    addChild(component, "image", "imgBottomBar", 985, 848, 930, 126, 'src="a000000d" fileName="qdao_bottom_bar.png"')
    addChild(component, "image", "imgRole", 1810, 18, 335, 332, 'src="a000000i" fileName="qdao_role_wanderer.png"')
    addChild(component, "image", "imgCloud", 1955, 850, 293, 160, 'src="a000000e" fileName="qdao_cloud_corner.png"')
    -- Static art is published by FairyGUI. The server list below is populated
    -- by C# using real zone data.
    addChild(component, "component", "listServer", 0, 0, DESIGN_W, DESIGN_H, '')
    addChild(component, "component", "tabRecent", 524, 280, 200, 78, '')
    addChild(component, "component", "tabRecommend", 524, 372, 200, 78, '')
    addChild(component, "component", "tabAll", 524, 464, 200, 78, '')
    addChild(component, "text", "txtStatus", 760, 818, 840, 40, 'text=""')
    addChild(component, "component", "btnBack", 565, 846, 210, 76, '')
    addChild(component, "component", "btnRefresh", 1430, 846, 210, 76, '')
    addChild(component, "component", "btnConfirm", 1668, 832, 300, 92, '')
    if doc ~= nil then doc:Save() end
end

local function generateQdaoScreens()
    local pkg = getQdaoPackage()
    if pkg == nil then return end

    pkg:BeginBatch()
    buildLogin(pkg)
    buildServerSelect(pkg)
    pkg:SetChanged()
    pkg:Save()
    pkg:EndBatch()
    FairyEditor.App.Alert("Generated qdao LoginScreen and ServerSelectScreen drafts. Review the components, set button extensions/states, then publish qdao to Unity.")
end

local toolMenu = FairyEditor.App.menu:GetSubMenu("tool")
toolMenu:AddItem("QDao/Generate login and server screens", "qdao_generate_login_server", function(menuItem)
    generateQdaoScreens()
end)

function onDestroy()
    toolMenu:RemoveItem("qdao_generate_login_server")
end