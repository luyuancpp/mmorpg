#!/usr/bin/env pwsh
# robot_stress.ps1 — Build robot image, generate per-zone configs, and launch K8s Jobs.
#
# Usage:
#   pwsh -File deploy/k8s/robot_stress.ps1 [-TotalRobots 1000] [-ZonesConfig deploy/k8s/zones.10zones.yaml]
#
param(
    [int]$TotalRobots = 1000,
    [string]$ZonesConfig = "",
    [string]$ImageName = "ghcr.io/luyuancpp/mmorpg-robot:latest",
    [string]$Password = "123456",
    [string]$AccountFmt = "robot_%04d",
    [string]$NamespacePrefix = "mmorpg-zone",
    [int]$ActionInterval = 3,
    [int]$ReportInterval = 5,
    [string]$Profile = "stress",
    [switch]$SkipBuild,
    [switch]$Delete
)

$ErrorActionPreference = "Stop"
$RepoRoot = Resolve-Path (Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) "..\..")

# --- Delete mode ---
if ($Delete) {
    Write-Host "=== Deleting robot jobs from all zone namespaces ==="
    $namespaces = kubectl get ns -o jsonpath='{.items[*].metadata.name}' | ForEach-Object { $_ -split '\s+' } | Where-Object { $_ -match "^$NamespacePrefix-" }
    foreach ($ns in $namespaces) {
        kubectl delete job robot-stress -n $ns --ignore-not-found 2>$null
        kubectl delete configmap robot-config -n $ns --ignore-not-found 2>$null
    }
    Write-Host "Done."
    return
}

# --- Build robot image ---
if (-not $SkipBuild) {
    Write-Host "=== Building robot Docker image ==="
    docker build -f "$RepoRoot\deploy\k8s\Dockerfile.robot" -t $ImageName "$RepoRoot"
    if ($LASTEXITCODE -ne 0) { throw "Robot image build failed" }

    # Load into containerd for Docker Desktop K8s
    Write-Host "=== Loading robot image into containerd ==="
    docker save $ImageName | docker exec -i desktop-control-plane ctr -n k8s.io images import --all-platforms -
    Write-Host "Robot image loaded."
}

# --- Discover zones ---
if ([string]::IsNullOrWhiteSpace($ZonesConfig)) {
    $ZonesConfig = Join-Path $RepoRoot "deploy\k8s\zones.10zones.yaml"
}

# Parse zones YAML (simple parser — expects "- name: xxx" lines)
$zonesContent = Get-Content $ZonesConfig -Raw
$zoneNames = [regex]::Matches($zonesContent, 'name:\s*(\S+)') | ForEach-Object { $_.Groups[1].Value }

if ($zoneNames.Count -eq 0) {
    throw "No zones found in $ZonesConfig"
}

$robotsPerZone = [math]::Floor($TotalRobots / $zoneNames.Count)
$remainder = $TotalRobots % $zoneNames.Count

Write-Host "=== Launching $TotalRobots robots across $($zoneNames.Count) zones ($robotsPerZone per zone) ==="

$accountOffset = 1
for ($i = 0; $i -lt $zoneNames.Count; $i++) {
    $zoneName = $zoneNames[$i]
    $ns = "$NamespacePrefix-$zoneName"
    $count = $robotsPerZone
    if ($i -lt $remainder) { $count++ }

    $startIdx = $accountOffset
    $accountOffset += $count

    # Robot connects to login service inside same namespace via K8s DNS
    $loginAddr = "login.$($ns):50000"

    $configYaml = @"
gate_mode: "grpc"
login_addr: "$loginAddr"
gateway_addr: ""
gate_addr: ""
zone_id: 0
robot_count: $count
account_fmt: "${AccountFmt}_zone${i}_"
password: "$Password"
skill_ids: [1001]
action_interval: $ActionInterval
report_interval: $ReportInterval
mode: "stress"
profile: "$Profile"
llm:
  enabled: false
"@

    # Create/update ConfigMap
    kubectl create configmap robot-config --from-literal=robot.yaml="$configYaml" -n $ns --dry-run=client -o yaml | kubectl apply -f - -n $ns

    # Create Job manifest
    $jobYaml = @"
apiVersion: batch/v1
kind: Job
metadata:
  name: robot-stress
  labels:
    app: robot-stress
spec:
  backoffLimit: 3
  template:
    metadata:
      labels:
        app: robot-stress
    spec:
      restartPolicy: OnFailure
      containers:
        - name: robot
          image: $ImageName
          imagePullPolicy: IfNotPresent
          args: ["-c", "/app/etc/robot.yaml"]
          volumeMounts:
            - name: config
              mountPath: /app/etc
              readOnly: true
          resources:
            requests:
              cpu: 500m
              memory: 256Mi
            limits:
              cpu: "2"
              memory: 1Gi
      volumes:
        - name: config
          configMap:
            name: robot-config
"@

    # Delete old job if exists, then create new
    kubectl delete job robot-stress -n $ns --ignore-not-found 2>$null
    $countStr = $count.ToString().PadLeft(4, '0')
    Write-Host "  [$ns] Launching $count robots (accounts: zone${i}_0001..zone${i}_$countStr)"
    $jobYaml | kubectl apply -f - -n $ns
}

Write-Host ""
Write-Host "=== Robot stress test launched ==="
Write-Host "Monitor: kubectl get jobs -A -l app=robot-stress"
Write-Host "Logs:    kubectl logs -f job/robot-stress -n $NamespacePrefix-$($zoneNames[0])"
Write-Host "Delete:  pwsh -File deploy/k8s/robot_stress.ps1 -Delete"
