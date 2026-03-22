param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("help", "pbgen-build", "pbgen-run", "proto-gen-build", "proto-gen-run", "tree", "naming-audit", "naming-apply", "third-party-grpc-build", "iwyu-run", "k8s-zone-up", "k8s-zone-down", "k8s-zone-status", "k8s-all-up", "k8s-all-down", "k8s-all-status", "k8s-stage-runtime", "k8s-image-preflight", "k8s-build-image", "k8s-push-image", "k8s-release-zone", "k8s-release-all")]
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
    [string]$KubeConfig = ""
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Resolve-Path (Join-Path $ScriptDir "..\..")
$ProtoGenDir = Join-Path $RepoRoot "tools\proto_generator\pbgen"

function Invoke-ProtoGenBuild {
    Push-Location $ProtoGenDir
    try {
        go build -v ./...
    }
    finally {
        Pop-Location
    }
}

function Invoke-ProtoGenRun {
    Push-Location $ProtoGenDir
    $previousConfigPath = $env:PROTO_GEN_CONFIG_PATH
    $previousEnablePprof = $env:PBGEN_ENABLE_PPROF
    try {
        if ($UseBinary -and $UseGoRun) {
            throw "UseBinary and UseGoRun cannot be enabled at the same time."
        }

        if ([string]::IsNullOrWhiteSpace($ConfigPath)) {
            $env:PROTO_GEN_CONFIG_PATH = Join-Path $ProtoGenDir "etc\proto_gen.yaml"
        }
        else {
            $env:PROTO_GEN_CONFIG_PATH = $ConfigPath
        }

        if ($EnablePprof) {
            $env:PBGEN_ENABLE_PPROF = "1"
        }
        else {
            Remove-Item Env:PBGEN_ENABLE_PPROF -ErrorAction SilentlyContinue
        }

        $protoGenExePath = Join-Path $ProtoGenDir "pbgen.exe"

        if ($UseGoRun) {
            go run ./cmd
            return
        }

        if ($UseBinary) {
            if (-not (Test-Path $protoGenExePath)) {
                throw "proto-gen binary not found: $protoGenExePath. Please run proto-gen-build first."
            }

            & $protoGenExePath
            return
        }

        # Default strategy: prefer prebuilt binary for faster startup, fallback to go run.
        if (Test-Path $protoGenExePath) {
            & $protoGenExePath
        }
        else {
            go run ./cmd
        }
    }
    finally {
        if ($null -ne $previousConfigPath) {
            $env:PROTO_GEN_CONFIG_PATH = $previousConfigPath
        }
        else {
            Remove-Item Env:PROTO_GEN_CONFIG_PATH -ErrorAction SilentlyContinue
        }

        if ($null -ne $previousEnablePprof) {
            $env:PBGEN_ENABLE_PPROF = $previousEnablePprof
        }
        else {
            Remove-Item Env:PBGEN_ENABLE_PPROF -ErrorAction SilentlyContinue
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
    -EnablePprof   Enable pprof wait mode (PBGEN_ENABLE_PPROF=1)
    -UseBinary     Force running proto-gen binary (pbgen.exe)
    -UseGoRun      Force running go run ./cmd

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
    default { throw "Unsupported command: $Command" }
}
