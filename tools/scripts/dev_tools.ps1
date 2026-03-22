param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("help", "pbgen-build", "pbgen-run", "proto-gen-build", "proto-gen-run", "tree", "naming-audit", "naming-apply", "third-party-grpc-build", "iwyu-run", "k8s-zone-up", "k8s-zone-down", "k8s-zone-status", "k8s-all-up", "k8s-all-down", "k8s-all-status", "k8s-stage-runtime", "k8s-image-preflight", "k8s-build-image", "k8s-push-image", "k8s-release-zone", "k8s-release-all", "go-svc-start", "go-svc-stop", "go-svc-status", "go-svc-list", "go-svc-build-images", "go-svc-push-images")]
    [string]$Command,

    [string]$ConfigPath = "",

    [switch]$EnablePprof,
    [switch]$UseBinary,
    [switch]$UseGoRun,

    [ValidateSet("snake", "kebab")]
    [string]$Style = "snake",

    [int]$MaxChanges = 0,
    [int]$Jobs = 0,

    [string]$ZoneName = "yesterday",
    [int]$ZoneId = 101,
    [string]$NamespacePrefix = "mmorpg-zone",
    [string]$ZonesConfigPath = "",
    [string]$NodeImage = "ghcr.io/luyuancpp/mmorpg-node:latest",
    [ValidateSet("custom", "managed-cloud", "bare-metal")]
    [string]$OpsProfile = "custom",
    [string]$ImageRepository = "ghcr.io/luyuancpp/mmorpg-node",
    [string]$ImageTag = "latest",
    [string]$RuntimeRoot = "deploy/k8s/runtime/linux",
    [string]$DockerfilePath = "deploy/k8s/Dockerfile.runtime",
    [string]$BinarySourceRoot = "",
    [string]$ZoneInfoSource = "bin/zoneinfo",
    [string]$TableSource = "generated/tables",
    [int]$CentreReplicas = 1,
    [int]$GateReplicas = 2,
    [int]$SceneReplicas = 4,
    [ValidateSet("ClusterIP", "NodePort", "LoadBalancer")]
    [string]$GateServiceType = "NodePort",
    [int]$GateServicePort = 18000,
    [switch]$SkipInfra,
    [switch]$SkipGoSvc,
    [string]$GoSvcRegistry = "ghcr.io/luyuancpp",
    [string]$GoSvcTag = "latest",
    [bool]$BuildRelease = $true,
    [bool]$BuildDebug = $true,
    # iwyu-run
    [string[]]$NodePath = @("cpp/nodes/scene"),
    [ValidateSet("auto", "iwyu", "clang-tidy")]
    [string]$IwyuTool = "auto",
    [switch]$FixIncludes,
    [switch]$ChangedOnly,
    [string]$DiffBase = "",
    [switch]$Clean,
    [switch]$SkipToolCheck,
    [switch]$DryRun,
    [switch]$WaitReady,
    [int]$WaitTimeoutSeconds = 180,
    [string]$KubeContext = "",
    [string]$KubeConfig = "",
    # go-svc-* commands
    [string[]]$GoServices = @()
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Resolve-Path (Join-Path $ScriptDir "..\..")
$ProtoGenDir = Join-Path $RepoRoot "tools\proto_generator\protogen"
$ProtoGenEnablePprofEnvVar = "PROTOGEN_ENABLE_PPROF"
$LegacyProtoGenEnablePprofEnvVar = "PBGEN_ENABLE_PPROF"

function Invoke-ProtoGenBuild {
    Push-Location $ProtoGenDir
    try {
        $legacyProtoGenExePath = Join-Path $ProtoGenDir "pbgen.exe"
        $primaryProtoGenExePath = Join-Path $ProtoGenDir "proto-gen.exe"
        $staleCmdExePath = Join-Path $ProtoGenDir "cmd.exe"

        go build -v -o $legacyProtoGenExePath ./cmd
        Copy-Item -Path $legacyProtoGenExePath -Destination $primaryProtoGenExePath -Force

        if (Test-Path $staleCmdExePath) {
            Remove-Item $staleCmdExePath -Force -ErrorAction SilentlyContinue
        }
    }
    finally {
        Pop-Location
    }
}

function Invoke-ProtoGenRun {
    Push-Location $ProtoGenDir
    $previousConfigPath = [Environment]::GetEnvironmentVariable("PROTO_GEN_CONFIG_PATH", "Process")
    $previousEnablePprof = [Environment]::GetEnvironmentVariable($ProtoGenEnablePprofEnvVar, "Process")
    $previousLegacyEnablePprof = [Environment]::GetEnvironmentVariable($LegacyProtoGenEnablePprofEnvVar, "Process")
    try {
        if ($UseBinary -and $UseGoRun) {
            throw "UseBinary and UseGoRun cannot be enabled at the same time."
        }

        if ([string]::IsNullOrWhiteSpace($ConfigPath)) {
            [Environment]::SetEnvironmentVariable("PROTO_GEN_CONFIG_PATH", (Join-Path $ProtoGenDir "etc\proto_gen.yaml"), "Process")
        }
        else {
            $resolvedConfigPath = $ConfigPath
            if (-not [System.IO.Path]::IsPathRooted($resolvedConfigPath)) {
                $resolvedConfigPath = Join-Path $RepoRoot $resolvedConfigPath
            }

            $resolvedConfigPath = (Resolve-Path $resolvedConfigPath).Path
            [Environment]::SetEnvironmentVariable("PROTO_GEN_CONFIG_PATH", $resolvedConfigPath, "Process")
        }

        if ($EnablePprof) {
            [Environment]::SetEnvironmentVariable($ProtoGenEnablePprofEnvVar, "1", "Process")
        }
        else {
            [Environment]::SetEnvironmentVariable($ProtoGenEnablePprofEnvVar, $null, "Process")
            [Environment]::SetEnvironmentVariable($LegacyProtoGenEnablePprofEnvVar, $null, "Process")
        }

        $primaryProtoGenExePath = Join-Path $ProtoGenDir "proto-gen.exe"
        $legacyProtoGenExePath = Join-Path $ProtoGenDir "pbgen.exe"

        if ($UseGoRun) {
            go run ./cmd
            return
        }

        if ($UseBinary) {
            if (Test-Path $primaryProtoGenExePath) {
                & $primaryProtoGenExePath
                return
            }

            if (Test-Path $legacyProtoGenExePath) {
                & $legacyProtoGenExePath
                return
            }

            throw "proto-gen binary not found. Expected $primaryProtoGenExePath or $legacyProtoGenExePath. Please run proto-gen-build first."
        }

        # Default strategy: prefer prebuilt proto-gen binary for faster startup, fall back to the legacy name, then go run.
        if (Test-Path $primaryProtoGenExePath) {
            & $primaryProtoGenExePath
        }
        elseif (Test-Path $legacyProtoGenExePath) {
            & $legacyProtoGenExePath
        }
        else {
            go run ./cmd
        }
    }
    finally {
        if ($null -ne $previousConfigPath) {
            [Environment]::SetEnvironmentVariable("PROTO_GEN_CONFIG_PATH", $previousConfigPath, "Process")
        }
        else {
            [Environment]::SetEnvironmentVariable("PROTO_GEN_CONFIG_PATH", $null, "Process")
        }

        if ($null -ne $previousEnablePprof) {
            [Environment]::SetEnvironmentVariable($ProtoGenEnablePprofEnvVar, $previousEnablePprof, "Process")
        }
        else {
            [Environment]::SetEnvironmentVariable($ProtoGenEnablePprofEnvVar, $null, "Process")
        }

        if ($null -ne $previousLegacyEnablePprof) {
            [Environment]::SetEnvironmentVariable($LegacyProtoGenEnablePprofEnvVar, $previousLegacyEnablePprof, "Process")
        }
        else {
            [Environment]::SetEnvironmentVariable($LegacyProtoGenEnablePprofEnvVar, $null, "Process")
        }

        Pop-Location
    }
}

function Invoke-Tree {
    $TreeScript = Join-Path $ScriptDir "tree.ps1"
    if (-not (Test-Path $TreeScript)) {
        throw "tree.ps1 not found: $TreeScript"
    }

    & $TreeScript
}

function Invoke-NamingAudit {
    $scriptPath = Join-Path $ScriptDir "normalize_names.ps1"
    if (-not (Test-Path $scriptPath)) {
        throw "normalize_names.ps1 not found: $scriptPath"
    }

    & $scriptPath -Mode audit -Style $Style -MaxChanges $MaxChanges
}

function Invoke-NamingApply {
    $scriptPath = Join-Path $ScriptDir "normalize_names.ps1"
    if (-not (Test-Path $scriptPath)) {
        throw "normalize_names.ps1 not found: $scriptPath"
    }

    & $scriptPath -Mode apply -Style $Style -MaxChanges $MaxChanges -Confirm:$false
}

function Invoke-ThirdPartyGrpcBuild {
    $scriptPath = Join-Path $ScriptDir "third_party\build_grpc.ps1"
    if (-not (Test-Path $scriptPath)) {
        throw "third_party/build_grpc.ps1 not found: $scriptPath"
    }

    $bound = $script:PSBoundParameters
    $args = @{}
    if ($bound.ContainsKey('BuildRelease')) {
        $args.BuildRelease = $BuildRelease
    }
    if ($bound.ContainsKey('BuildDebug')) {
        $args.BuildDebug = $BuildDebug
    }
    if ($bound.ContainsKey('Jobs')) {
        $args.Jobs = $Jobs
    }
    if ($Clean) {
        $args.Clean = $true
    }
    if ($SkipToolCheck) {
        $args.SkipToolCheck = $true
    }

    & $scriptPath @args
}

function Invoke-IwyuRun {
    $scriptPath = Join-Path $ScriptDir "iwyu_run.ps1"
    if (-not (Test-Path $scriptPath)) {
        throw "iwyu_run.ps1 not found: $scriptPath"
    }

    $args = @{
        NodePath = $NodePath
        Tool     = $IwyuTool
    }
    if ($FixIncludes) { $args.Fix = $true }
    if ($ChangedOnly) { $args.ChangedOnly = $true }
    if (-not [string]::IsNullOrWhiteSpace($DiffBase)) { $args.DiffBase = $DiffBase }

    & $scriptPath @args
}

function Invoke-K8sDeploy {
    param(
        [Parameter(Mandatory = $true)]
        [string]$K8sCommand
    )

    $scriptPath = Join-Path $ScriptDir "k8s_deploy.ps1"
    if (-not (Test-Path $scriptPath)) {
        throw "k8s_deploy.ps1 not found: $scriptPath"
    }

    $args = @{
        Command = $K8sCommand
        ZoneName = $ZoneName
        ZoneId = $ZoneId
        NamespacePrefix = $NamespacePrefix
        NodeImage = $NodeImage
        OpsProfile = $OpsProfile
        CentreReplicas = $CentreReplicas
        GateReplicas = $GateReplicas
        SceneReplicas = $SceneReplicas
        GateServiceType = $GateServiceType
        GateServicePort = $GateServicePort
        WaitTimeoutSeconds = $WaitTimeoutSeconds
    }

    if (-not [string]::IsNullOrWhiteSpace($ZonesConfigPath)) {
        $args.ZonesConfigPath = $ZonesConfigPath
    }

    if (-not [string]::IsNullOrWhiteSpace($KubeContext)) {
        $args.KubeContext = $KubeContext
    }

    if (-not [string]::IsNullOrWhiteSpace($KubeConfig)) {
        $args.KubeConfig = $KubeConfig
    }

    if ($SkipInfra) {
        $args.SkipInfra = $true
    }

    if ($SkipGoSvc) {
        $args.SkipGoSvc = $true
    }

    if (-not [string]::IsNullOrWhiteSpace($GoSvcRegistry)) {
        $args.GoSvcRegistry = $GoSvcRegistry
        $args.GoSvcTag = $GoSvcTag
    }

    if ($DryRun) {
        $args.DryRun = $true
    }

    if ($WaitReady) {
        $args.WaitReady = $true
    }

    & $scriptPath @args
}

function Invoke-K8sImage {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ImageCommand
    )

    $scriptPath = Join-Path $ScriptDir "k8s_image.ps1"
    if (-not (Test-Path $scriptPath)) {
        throw "k8s_image.ps1 not found: $scriptPath"
    }

    $args = @{
        Command = $ImageCommand
        RuntimeRoot = $RuntimeRoot
        DockerfilePath = $DockerfilePath
        ImageRepository = $ImageRepository
        ImageTag = $ImageTag
        ZoneName = $ZoneName
        ZoneId = $ZoneId
        NamespacePrefix = $NamespacePrefix
        OpsProfile = $OpsProfile
        CentreReplicas = $CentreReplicas
        GateReplicas = $GateReplicas
        SceneReplicas = $SceneReplicas
        GateServiceType = $GateServiceType
        GateServicePort = $GateServicePort
        WaitTimeoutSeconds = $WaitTimeoutSeconds
    }

    if (-not [string]::IsNullOrWhiteSpace($ZonesConfigPath)) {
        $args.ZonesConfigPath = $ZonesConfigPath
    }
    if (-not [string]::IsNullOrWhiteSpace($KubeContext)) {
        $args.KubeContext = $KubeContext
    }
    if (-not [string]::IsNullOrWhiteSpace($KubeConfig)) {
        $args.KubeConfig = $KubeConfig
    }
    if ($SkipInfra) {
        $args.SkipInfra = $true
    }
    if ($WaitReady) {
        $args.WaitReady = $true
    }
    if ($DryRun) {
        $args.DryRun = $true
    }

    & $scriptPath @args
}

function Invoke-K8sStageRuntime {
    $scriptPath = Join-Path $ScriptDir "k8s_stage_runtime.ps1"
    if (-not (Test-Path $scriptPath)) {
        throw "k8s_stage_runtime.ps1 not found: $scriptPath"
    }

    if ([string]::IsNullOrWhiteSpace($BinarySourceRoot)) {
        throw "-BinarySourceRoot is required for k8s-stage-runtime"
    }

    & $scriptPath `
        -BinarySourceRoot $BinarySourceRoot `
        -RuntimeRoot $RuntimeRoot `
        -ZoneInfoSource $ZoneInfoSource `
        -TableSource $TableSource
}

function Invoke-Help {
    @"
dev_tools.ps1 command help

Primary proto generator commands:
    -Command proto-gen-build
    -Command proto-gen-run

Compatibility aliases (legacy):
    -Command pbgen-build
    -Command pbgen-run

Useful proto-gen flags:
    -EnablePprof   Enable pprof wait mode (PROTOGEN_ENABLE_PPROF=1)
    -UseBinary     Force running proto-gen binary (proto-gen.exe, fallback: pbgen.exe)
    -UseGoRun      Force running go run ./cmd

Go micro-service commands (local dev):
    -Command go-svc-start [-GoServices login,db,...]
    -Command go-svc-stop  [-GoServices login,...]
    -Command go-svc-status
    -Command go-svc-list

Go micro-service Docker image commands:
    -Command go-svc-build-images [-GoSvcRegistry <registry> -GoSvcTag <tag>]
    -Command go-svc-push-images  [-GoSvcRegistry <registry> -GoSvcTag <tag>]

Other common commands:
    -Command tree
    -Command naming-audit
    -Command naming-apply
    -Command third-party-grpc-build
    -Command iwyu-run
    -Command k8s-zone-up | k8s-zone-down | k8s-zone-status
    -Command k8s-all-up | k8s-all-down | k8s-all-status
    -Command k8s-stage-runtime
    -Command k8s-image-preflight | k8s-build-image | k8s-push-image | k8s-release-zone | k8s-release-all

Proto-gen naming docs:
    tools/docs/proto_gen_naming_audit.md
    tools/docs/proto_gen_naming_migration.md
"@ | Write-Output
}

switch ($Command) {
    "help" { Invoke-Help }
    "pbgen-build" { Invoke-ProtoGenBuild }
    "pbgen-run" { Invoke-ProtoGenRun }
    "proto-gen-build" { Invoke-ProtoGenBuild }
    "proto-gen-run" { Invoke-ProtoGenRun }
    "tree" { Invoke-Tree }
    "naming-audit" { Invoke-NamingAudit }
    "naming-apply" { Invoke-NamingApply }
    "third-party-grpc-build" { Invoke-ThirdPartyGrpcBuild }
    "iwyu-run"              { Invoke-IwyuRun }
    "k8s-zone-up" { Invoke-K8sDeploy -K8sCommand "zone-up" }
    "k8s-zone-down" { Invoke-K8sDeploy -K8sCommand "zone-down" }
    "k8s-zone-status" { Invoke-K8sDeploy -K8sCommand "zone-status" }
    "k8s-all-up" { Invoke-K8sDeploy -K8sCommand "all-up" }
    "k8s-all-down" { Invoke-K8sDeploy -K8sCommand "all-down" }
    "k8s-all-status" { Invoke-K8sDeploy -K8sCommand "all-status" }
    "k8s-stage-runtime" { Invoke-K8sStageRuntime }
    "k8s-image-preflight" { Invoke-K8sImage -ImageCommand "preflight" }
    "k8s-build-image" { Invoke-K8sImage -ImageCommand "build-image" }
    "k8s-push-image" { Invoke-K8sImage -ImageCommand "push-image" }
    "k8s-release-zone" { Invoke-K8sImage -ImageCommand "release-zone" }
    "k8s-release-all" { Invoke-K8sImage -ImageCommand "release-all" }
    "go-svc-start"  { & (Join-Path $ScriptDir "go_services.ps1") -Command start  -Services $GoServices }
    "go-svc-stop"   { & (Join-Path $ScriptDir "go_services.ps1") -Command stop   -Services $GoServices }
    "go-svc-status" { & (Join-Path $ScriptDir "go_services.ps1") -Command status }
    "go-svc-list"   { & (Join-Path $ScriptDir "go_services.ps1") -Command list   }
    "go-svc-build-images" { & (Join-Path $ScriptDir "go_svc_image.ps1") -Command build-all -Registry $GoSvcRegistry -Tag $GoSvcTag -DryRun:$DryRun }
    "go-svc-push-images"  { & (Join-Path $ScriptDir "go_svc_image.ps1") -Command push-all  -Registry $GoSvcRegistry -Tag $GoSvcTag -DryRun:$DryRun }
    default { throw "Unsupported command: $Command" }
}
