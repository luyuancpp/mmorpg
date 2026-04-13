param(
	[Parameter(Mandatory = $true)]
	[ValidateSet("zone-up", "zone-down", "zone-status", "all-up", "all-down", "all-status", "infra-up", "infra-down", "infra-status")]
	[string]$Command,

	[string]$ZoneName = "yesterday",
	[int]$ZoneId = 101,
	[string]$NamespacePrefix = "mmorpg-zone",
	[string]$InfraNamespace = "mmorpg-infra",

	[string]$ZonesConfigPath = "",

	[string]$NodeImage = "ghcr.io/luyuancpp/mmorpg-node:latest",
	[ValidateSet("custom", "managed-cloud", "bare-metal")]
	[string]$OpsProfile = "custom",
	[ValidateSet("dev", "prod-like", "prod")]
	[string]$KafkaProfile = "prod",
	[int]$KafkaBrokerRetentionMs = 0,
	[int]$KafkaDbTaskRetentionMs = 0,
	[int]$KafkaRetentionCheckIntervalMs = 0,
	[long]$KafkaRetentionBytes = 0,
	[long]$KafkaSegmentBytes = 0,
	[string]$KafkaHeapOpts = "",
	[int]$CentreReplicas = 1,
	[int]$GateReplicas = 2,
	[int]$SceneReplicas = 4,
	[int]$GrpcThreadPoolReserveThreads = 1,
	[ValidateSet("ClusterIP", "NodePort", "LoadBalancer")]
	[string]$GateServiceType = "NodePort",
	[int]$GateServicePort = 18000,

	[switch]$SkipInfra,
	[switch]$SkipGoSvc,
	[string]$GoSvcRegistry = "",
	[string]$GoSvcTag = "latest",
	[switch]$SkipJavaSvc,
	[string]$JavaSvcRegistry = "",
	[string]$JavaSvcTag = "latest",
	[switch]$DryRun,
	[switch]$WaitReady,
	[int]$WaitTimeoutSeconds = 180,

	[string]$KubeContext = "",
	[string]$KubeConfig = ""
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Resolve-Path (Join-Path $ScriptDir "..\..")
$K8sRoot = Join-Path $RepoRoot "deploy\k8s"
$InfraManifestsDir = Join-Path $K8sRoot "manifests\infra"
$GoSvcManifestsDir = Join-Path $K8sRoot "manifests\go-svc"
$JavaSvcManifestsDir = Join-Path $K8sRoot "manifests\java-svc"

# Go micro-service catalogue: name → { configMapName, manifestFile, port, configFlag, configFileName }
$GoSvcCatalogue = @{
	db              = @{ ConfigMap = "go-svc-db-config";              Manifest = "db.yaml";              Port = 6000;  ConfigFlag = "-f";              ConfigFile = "db.yaml";                    ImageName = "mmorpg-db" }
	"data-service"  = @{ ConfigMap = "go-svc-data-service-config";    Manifest = "data-service.yaml";    Port = 9000;  ConfigFlag = "-f";              ConfigFile = "data_service.yaml";             ImageName = "mmorpg-data-service" }
	login           = @{ ConfigMap = "go-svc-login-config";           Manifest = "login.yaml";           Port = 50000; ConfigFlag = "-loginService";   ConfigFile = "login.yaml";                  ImageName = "mmorpg-login" }
	"player-locator"= @{ ConfigMap = "go-svc-player-locator-config";  Manifest = "player-locator.yaml";  Port = 50100; ConfigFlag = "-f";              ConfigFile = "player_locator.yaml";           ImageName = "mmorpg-player-locator" }
	"scene-manager" = @{ ConfigMap = "go-svc-scene-manager-config";   Manifest = "scene-manager.yaml";   Port = 60000; ConfigFlag = "-f";              ConfigFile = "scene_manager_service.yaml";    ImageName = "mmorpg-scene-manager" }
	gateway         = @{ ConfigMap = "go-svc-gateway-config";         Manifest = "gateway.yaml";         Port = 8080;  ConfigFlag = "-f";              ConfigFile = "gateway.yaml";                  ImageName = "mmorpg-gateway" }
}

# Java service catalogue
$JavaSvcCatalogue = @{
	auth = @{ ConfigMap = "java-svc-auth-config"; Manifest = "auth.yaml"; HttpPort = 5555; GrpcPort = 5556; ImageName = "mmorpg-auth" }
}

function Apply-OpsProfileDefaults {
	switch ($OpsProfile) {
		"managed-cloud" {
			$script:GateServiceType = "LoadBalancer"
			if ($CentreReplicas -lt 1) { $script:CentreReplicas = 1 }
			if ($GateReplicas -lt 2) { $script:GateReplicas = 2 }
			if ($SceneReplicas -lt 4) { $script:SceneReplicas = 4 }
		}
		"bare-metal" {
			$script:GateServiceType = "NodePort"
			if ($CentreReplicas -lt 1) { $script:CentreReplicas = 1 }
			if ($GateReplicas -lt 2) { $script:GateReplicas = 2 }
			if ($SceneReplicas -lt 4) { $script:SceneReplicas = 4 }
		}
		default {
		}
	}
}

function Apply-KafkaProfileDefaults {
	switch ($KafkaProfile) {
		"dev" {
			if ($KafkaBrokerRetentionMs -le 0) { $script:KafkaBrokerRetentionMs = 60000 }
			if ($KafkaDbTaskRetentionMs -le 0) { $script:KafkaDbTaskRetentionMs = 300000 }
			if ($KafkaRetentionCheckIntervalMs -le 0) { $script:KafkaRetentionCheckIntervalMs = 120000 }
			if ($KafkaRetentionBytes -le 0) { $script:KafkaRetentionBytes = 134217728 }
			if ($KafkaSegmentBytes -le 0) { $script:KafkaSegmentBytes = 16777216 }
			if ([string]::IsNullOrWhiteSpace($KafkaHeapOpts)) { $script:KafkaHeapOpts = "-Xms128m -Xmx256m" }
		}
		"prod-like" {
			if ($KafkaBrokerRetentionMs -le 0) { $script:KafkaBrokerRetentionMs = 300000 }
			if ($KafkaDbTaskRetentionMs -le 0) { $script:KafkaDbTaskRetentionMs = 600000 }
			if ($KafkaRetentionCheckIntervalMs -le 0) { $script:KafkaRetentionCheckIntervalMs = 120000 }
			if ($KafkaRetentionBytes -le 0) { $script:KafkaRetentionBytes = 536870912 }
			if ($KafkaSegmentBytes -le 0) { $script:KafkaSegmentBytes = 33554432 }
			if ([string]::IsNullOrWhiteSpace($KafkaHeapOpts)) { $script:KafkaHeapOpts = "-Xms512m -Xmx1g" }
		}
		default {
			if ($KafkaBrokerRetentionMs -le 0) { $script:KafkaBrokerRetentionMs = 300000 }
			if ($KafkaDbTaskRetentionMs -le 0) { $script:KafkaDbTaskRetentionMs = 900000 }
			if ($KafkaRetentionCheckIntervalMs -le 0) { $script:KafkaRetentionCheckIntervalMs = 120000 }
			if ($KafkaRetentionBytes -le 0) { $script:KafkaRetentionBytes = 536870912 }
			if ($KafkaSegmentBytes -le 0) { $script:KafkaSegmentBytes = 33554432 }
			if ([string]::IsNullOrWhiteSpace($KafkaHeapOpts)) { $script:KafkaHeapOpts = "-Xms512m -Xmx1g" }
		}
	}
}

function Show-ExposureProfileWarning {
	if ($OpsProfile -eq "custom" -and $GateServiceType -eq "LoadBalancer") {
		Write-Warning "Using OpsProfile=custom with GateServiceType=LoadBalancer. Ensure your cluster has a mature LB implementation; otherwise prefer NodePort + external L4 load balancer or use -OpsProfile bare-metal."
	}
}

function Build-KubectlBaseArgs {
	$args = @()
	if (-not [string]::IsNullOrWhiteSpace($KubeContext)) {
		$args += @("--context", $KubeContext)
	}

	if (-not [string]::IsNullOrWhiteSpace($KubeConfig)) {
		$args += @("--kubeconfig", $KubeConfig)
	}

	return ,$args
}

function Invoke-Kubectl {
	param(
		[Parameter(Mandatory = $true)]
		[string[]]$Args,
		[switch]$AllowFailure
	)

	$baseArgs = Build-KubectlBaseArgs
	$allArgs = @()
	$allArgs += $baseArgs
	$allArgs += $Args

	if ($DryRun) {
		Write-Host "[dry-run] kubectl $($allArgs -join ' ')"
		return
	}

	& kubectl @allArgs
	if (-not $AllowFailure -and $LASTEXITCODE -ne 0) {
		throw "kubectl failed: kubectl $($allArgs -join ' ')"
	}
}

function Invoke-KubectlWithInputFile {
	param(
		[Parameter(Mandatory = $true)]
		[string[]]$Args,

		[Parameter(Mandatory = $true)]
		[string]$InputContent
	)

	$tempFile = [System.IO.Path]::GetTempFileName()
	try {
		$sanitized = $InputContent -replace "`t", "    "
		Set-Content -Path $tempFile -Value $sanitized -NoNewline -Encoding utf8NoBOM
		Invoke-Kubectl -Args ($Args + @("-f", $tempFile))
	}
	finally {
		Remove-Item -Path $tempFile -Force -ErrorAction SilentlyContinue
	}
}

function Get-ZoneNamespace {
	param([Parameter(Mandatory = $true)][string]$Name)
	return "{0}-{1}" -f $NamespacePrefix, $Name
}

function Ensure-KubectlAvailable {
	if ($DryRun) {
		return
	}

	$kubectl = Get-Command kubectl -ErrorAction SilentlyContinue
	if ($null -eq $kubectl) {
		throw "kubectl not found. Please install kubectl and configure cluster access first."
	}
}

function Ensure-Namespace {
	param([Parameter(Mandatory = $true)][string]$Namespace)

	$nsYaml = @"
apiVersion: v1
kind: Namespace
metadata:
  name: $Namespace
"@
	Invoke-KubectlWithInputFile -Args @("apply") -InputContent $nsYaml
}

function New-NodeConfigMapYaml {
	param(
		[Parameter(Mandatory = $true)][int]$CurrentZoneId,
		[Parameter(Mandatory = $true)][string]$ConfigName
	)

	$baseDeployConfig = (@"
Etcd:
  Hosts:
    - "etcd.${InfraNamespace}:2379"
  KeepaliveInterval: 1
  NodeTTLSeconds: 60
TableDataDirectory: "../generated/generated_tables/"
LogLevel: 1
HealthCheckInterval: 1
service_discovery_prefixes:
  - "SceneNodeService.rpc"
  - "GateNodeService.rpc"
  - "LoginNodeService.rpc"
Kafka:
  Brokers:
    - "kafka.${InfraNamespace}:9092"
  Topics:
    - "game-events"
  GroupID: "game-consumer-group"
  EnableAutoCommit: true
  AutoOffsetReset: "earliest"
"@) -replace "`t", "  "

	$gameConfig = @"
SceneNodeType: 0
ZoneId: $CurrentZoneId
zoneredis:
  host: "redis.${InfraNamespace}"
  port: 6379
  password: ""
  db: 0
  timeout: 3000
  max_connections: 100
  retry_interval: 1000
"@

	return @"
apiVersion: v1
kind: ConfigMap
metadata:
  name: $ConfigName
data:
  base_deploy_config.yaml: |
$($baseDeployConfig -split "`n" | ForEach-Object { "    $_" } | Out-String)
  game_config.yaml: |
$($gameConfig -split "`n" | ForEach-Object { "    $_" } | Out-String)
"@
}

function New-NodeDeploymentYaml {
	param(
		[Parameter(Mandatory = $true)][string]$NodeName,
		[Parameter(Mandatory = $true)][int]$Replicas,
		[Parameter(Mandatory = $true)][int]$RpcPort,
		[Parameter(Mandatory = $true)][string]$StartCommand,
		[Parameter(Mandatory = $true)][string]$ConfigMapName
	)

	$grpcEnvBlock = ""
	if ($NodeName -eq "gate" -and $GrpcThreadPoolReserveThreads -gt 0) {
		$grpcEnvBlock = @"
			- name: GRPC_THREAD_POOL_RESERVE_THREADS
			  value: "$GrpcThreadPoolReserveThreads"
"@
	}

	return @"
apiVersion: apps/v1
kind: Deployment
metadata:
  name: $NodeName
spec:
  replicas: $Replicas
  selector:
	matchLabels:
	  app: $NodeName
  template:
	metadata:
	  labels:
		app: $NodeName
	spec:
	  containers:
		- name: $NodeName
		  image: $NodeImage
		  imagePullPolicy: IfNotPresent
		  workingDir: /app/bin
		  command: ["/bin/sh", "-lc"]
		  args: ["$StartCommand"]
		  env:
			- name: POD_IP
			  valueFrom:
				fieldRef:
				  fieldPath: status.podIP
			- name: RPC_PORT
			  value: "$RpcPort"
			- name: NODE_PORT
			  value: "$RpcPort"
$grpcEnvBlock
		  volumeMounts:
			- name: node-config
			  mountPath: /app/bin/etc
			  readOnly: true
			- name: node-logs
			  mountPath: /app/bin/logs
		  ports:
			- containerPort: $RpcPort
			  name: rpc
	  volumes:
		- name: node-config
		  configMap:
			name: $ConfigMapName
		- name: node-logs
		  emptyDir: {}
"@
}

function New-GateServiceYaml {
	param(
		[Parameter(Mandatory = $true)][string]$ServiceName
	)

	return @"
apiVersion: v1
kind: Service
metadata:
  name: $ServiceName
spec:
  type: $GateServiceType
  selector:
    app: gate
  ports:
    - name: tcp-gate
      protocol: TCP
      port: $GateServicePort
      targetPort: rpc
"@
}

function Wait-ForDeploymentReady {
	param(
		[Parameter(Mandatory = $true)][string]$Namespace,
		[Parameter(Mandatory = $true)][string]$DeploymentName
	)

	if ($DryRun) {
		Write-Host "[dry-run] kubectl rollout status deployment/$DeploymentName -n $Namespace --timeout ${WaitTimeoutSeconds}s"
		return
	}

	Invoke-Kubectl -Args @("rollout", "status", "deployment/$DeploymentName", "-n", $Namespace, "--timeout", ("{0}s" -f $WaitTimeoutSeconds)) -AllowFailure
	if ($LASTEXITCODE -eq 0) {
		return
	}

	Write-Host "Deployment not ready: namespace=$Namespace deployment=$DeploymentName"
	Invoke-Kubectl -Args @("get", "pods", "-n", $Namespace, "-o", "wide") -AllowFailure
	Invoke-Kubectl -Args @("describe", "deployment", $DeploymentName, "-n", $Namespace) -AllowFailure
	throw "Deployment rollout failed: namespace=$Namespace deployment=$DeploymentName"
}

function Wait-ForZoneReady {
	param([Parameter(Mandatory = $true)][string]$Namespace)

	if (-not $WaitReady) {
		return
	}

	Write-Host "Waiting for zone workloads to become ready: namespace=$Namespace"
	Wait-ForDeploymentReady -Namespace $Namespace -DeploymentName "gate"
	Wait-ForDeploymentReady -Namespace $Namespace -DeploymentName "scene"

	if (-not $SkipGoSvc -and -not [string]::IsNullOrWhiteSpace($GoSvcRegistry)) {
		foreach ($svcName in $GoSvcCatalogue.Keys) {
			Wait-ForDeploymentReady -Namespace $Namespace -DeploymentName $svcName
		}
	}
	if (-not $SkipJavaSvc -and -not [string]::IsNullOrWhiteSpace($JavaSvcRegistry)) {
		foreach ($svcName in $JavaSvcCatalogue.Keys) {
			Wait-ForDeploymentReady -Namespace $Namespace -DeploymentName $svcName
		}
	}
	Write-Host "Zone workloads are ready: namespace=$Namespace"
}

function New-GoSvcConfigMapYaml {
	param(
		[Parameter(Mandatory = $true)][string]$SvcName,
		[Parameter(Mandatory = $true)][int]$CurrentZoneId
	)

	$info = $GoSvcCatalogue[$SvcName]
	$configMapName = $info.ConfigMap
	$configFileName = $info.ConfigFile
	$dbTaskRetentionMs = $script:KafkaDbTaskRetentionMs

	$svcConfig = switch ($SvcName) {
		"db" {
@"
Name: db.rpc
ListenOn: 0.0.0.0:6000
Etcd:
  Hosts:
    - "etcd.${InfraNamespace}:2379"
  Key: db.rpc
ZoneId: ${CurrentZoneId}
ServerConfig:
  JsonPath: "/app/data/mysql_database_table_list.json"
  Kafka:
    Brokers:
      - "kafka.${InfraNamespace}:9092"
    GroupID: "db_rpc_consumer_group"
    PartitionCnt: 5
    IsOfflineExpand: false
  Database:
    Hosts: "mysql.${InfraNamespace}:3306"
    User: "root"
    Passwd: "root"
    MaxOpenConn: 10
    MaxIdleConn: 3
    Net: ""
  RedisClient:
    Hosts: "redis.${InfraNamespace}:6379"
    DefaultTTLSeconds: 3600
    Password: ""
    DB: 0
"@
		}
		"data-service" {
@"
Name: dataservice.rpc
ListenOn: 0.0.0.0:9000
Etcd:
  Hosts:
    - "etcd.${InfraNamespace}:2379"
  Key: dataservice.rpc
MappingRedis:
  Host: redis.${InfraNamespace}:6379
  Type: node
  DB: 15
Regions:
  - Id: 1
    Zones: [1]
    Redis:
      Addrs:
        - redis.${InfraNamespace}:6379
DevRedis:
  Host: redis.${InfraNamespace}:6379
  Type: node
  DB: 0
PlayerLockTTLSec: 3
"@
		}
		"login" {
@"
Name: login.rpc
ListenOn: 0.0.0.0:50000
Timeout: 100000
Etcd:
  Hosts:
    - "etcd.${InfraNamespace}:2379"
  Key: login.rpc
Node:
  ZoneId: ${CurrentZoneId}
  SessionExpireMin: 1
  MaxLoginDevices: 3
  LeaseTTL: 500
  QueueShardCount: 50
  MaxLoginDuration: 5m
  LogoutGraceTime: 5s
  RedisClient:
    Host: redis.${InfraNamespace}:6379
    Password: ""
    DB: 0
    DefaultTTL: 24h
    DialTimeout: 3s
    ReadTimeout: 3s
    WriteTimeout: 3s
Snowflake:
  Epoch: 1721473263000
  NodeBits: 13
  StepBits: 9
Locker:
  AccountLockTTL: 10
  PlayerLockTTL: 5
Account:
  MaxDevicesPerAccount: 3
  CacheExpire: 12h
Registry:
  Etcd:
    Hosts:
      - "etcd.${InfraNamespace}:2379"
    Key: loginservice.rpc
    DialTimeout: 5s
Timeouts:
  EtcdDialTimeout: 5s
  ServiceDiscoveryTimeout: 10s
  TaskWaitTimeout: 5s
  LoginTotalTimeout: 10s
  RoleCacheExpire: 24h
  TaskManagerCleanInterval: 5s
  TaskBatchExpireTime: 10s
PlayerLocatorRpc:
  Etcd:
    Hosts:
      - "etcd.${InfraNamespace}:2379"
    Key: playerlocator.rpc
  Timeout: 5000
  Middlewares:
    Breaker: false
SceneManagerRpc:
  Etcd:
    Hosts:
      - "etcd.${InfraNamespace}:2379"
    Key: scenemanagerservice.rpc
  Timeout: 5000
  Middlewares:
    Breaker: false
GateTokenSecret: "change-me-in-production-use-a-strong-random-key"
Kafka:
  Brokers:
    - "kafka.${InfraNamespace}:9092"
  GroupID: "db_rpc_consumer_group"
  PartitionCnt: 5
  InitialPartition: 5
  DialTimeout: 10s
  ReadTimeout: 30s
  WriteTimeout: 10s
  RetryMax: 3
  RetryBackoff: 100ms
  ChannelBuffer: 1024
  SyncInterval: 30s
  StatsInterval: 5m
  CompressionType: 0
  Idempotent: true
  MaxOpenRequests: 1
	RetentionMs: ${dbTaskRetentionMs}
"@
		}
		"player-locator" {
@"
Name: playerlocator.rpc
ListenOn: 0.0.0.0:50100
Timeout: 10000
Etcd:
  Hosts:
    - "etcd.${InfraNamespace}:2379"
  Key: playerlocator.rpc
RedisClient:
  Host: redis.${InfraNamespace}:6379
  Password: ""
  DB: 0
Kafka:
  Brokers:
    - "kafka.${InfraNamespace}:9092"
Node:
  ZoneId: ${CurrentZoneId}
  LeaseTTL: 500
Registry:
  Etcd:
    Hosts:
      - "etcd.${InfraNamespace}:2379"
    DialTimeout: 5s
Lease:
  DefaultTTLSeconds: 30
  PollInterval: 1s
  BatchSize: 100
"@
		}
		"scene-manager" {
@"
Name: scenemanagerservice.rpc
ListenOn: 0.0.0.0:60000
Etcd:
  Hosts:
    - "etcd.${InfraNamespace}:2379"
  Key: scenemanagerservice.rpc
Redis:
  Host: redis.${InfraNamespace}:6379
  Type: node
  Key: scenemanagerservice
Kafka:
  Brokers:
    - "kafka.${InfraNamespace}:9092"
NodeID: "node-1"
"@
		}
		"gateway" {
@"
RestConf:
  Name: gateway
  Host: 0.0.0.0
  Port: 8080
  Timeout: 10000
Etcd:
  Hosts:
    - "etcd.${InfraNamespace}:2379"
  DialTimeout: 5s
Gate:
  TokenSecret: "change-me-in-production-use-a-strong-random-key"
  DiscoveryTimeout: 5s
"@
		}
		default {
			throw "Unknown Go service: $SvcName"
		}
	}

	return @"
apiVersion: v1
kind: ConfigMap
metadata:
  name: $configMapName
data:
  ${configFileName}: |
$($svcConfig -split "`n" | ForEach-Object { "    $_" } | Out-String)
"@
}

function Apply-GoSvcManifests {
	param(
		[Parameter(Mandatory = $true)][string]$Namespace,
		[Parameter(Mandatory = $true)][int]$CurrentZoneId
	)

	if ($SkipGoSvc) { return }
	if ([string]::IsNullOrWhiteSpace($GoSvcRegistry)) {
		Write-Host "[skip] Go services: -GoSvcRegistry not set, skipping Go service deployment."
		return
	}

	Write-Host "Applying Go micro-service manifests to namespace $Namespace (registry=$GoSvcRegistry tag=$GoSvcTag)"

	foreach ($svcName in $GoSvcCatalogue.Keys) {
		$info = $GoSvcCatalogue[$svcName]
		$svcImage = "$GoSvcRegistry/$($info.ImageName):$GoSvcTag"

		# Apply ConfigMap
		$cmYaml = New-GoSvcConfigMapYaml -SvcName $svcName -CurrentZoneId $CurrentZoneId
		Invoke-KubectlWithInputFile -Args @("apply", "-n", $Namespace) -InputContent $cmYaml

		# Apply manifest with image placeholder replaced
		$manifestPath = Join-Path $GoSvcManifestsDir $info.Manifest
		if (-not (Test-Path $manifestPath)) {
			Write-Warning "Go service manifest not found: $manifestPath – skipping $svcName"
			continue
		}
		$manifestContent = (Get-Content $manifestPath -Raw) -replace 'PLACEHOLDER_IMAGE', $svcImage
		Invoke-KubectlWithInputFile -Args @("apply", "-n", $Namespace) -InputContent $manifestContent

		Write-Host "  [applied] $svcName -> $svcImage (port $($info.Port))"
	}
}

function New-JavaSvcConfigMapYaml {
	param(
		[Parameter(Mandatory = $true)][string]$SvcName
	)

	$info = $JavaSvcCatalogue[$SvcName]
	$configMapName = $info.ConfigMap

	$svcConfig = switch ($SvcName) {
		"auth" {
@"
server:
  port: 5555
spring:
  application:
    name: sa-token-auth
  cloud:
    nacos:
      server-addr: nacos:8848
  data:
    redis:
      host: redis.${InfraNamespace}
sa-token:
  is-read-cookie: false
grpc:
  server:
    port: 5556
"@
		}
		default {
			throw "Unknown Java service: $SvcName"
		}
	}

	return @"
apiVersion: v1
kind: ConfigMap
metadata:
  name: $configMapName
data:
  application.yaml: |
$($svcConfig -split "`n" | ForEach-Object { "    $_" } | Out-String)
"@
}

function Apply-JavaSvcManifests {
	param(
		[Parameter(Mandatory = $true)][string]$Namespace
	)

	if ($SkipJavaSvc) { return }
	if ([string]::IsNullOrWhiteSpace($JavaSvcRegistry)) {
		Write-Host "[skip] Java services: -JavaSvcRegistry not set, skipping Java service deployment."
		return
	}

	Write-Host "Applying Java service manifests to namespace $Namespace (registry=$JavaSvcRegistry tag=$JavaSvcTag)"

	foreach ($svcName in $JavaSvcCatalogue.Keys) {
		$info = $JavaSvcCatalogue[$svcName]
		$svcImage = "$JavaSvcRegistry/$($info.ImageName):$JavaSvcTag"

		# Apply ConfigMap
		$cmYaml = New-JavaSvcConfigMapYaml -SvcName $svcName
		Invoke-KubectlWithInputFile -Args @("apply", "-n", $Namespace) -InputContent $cmYaml

		# Apply manifest with image placeholder replaced
		$manifestPath = Join-Path $JavaSvcManifestsDir $info.Manifest
		if (-not (Test-Path $manifestPath)) {
			Write-Warning "Java service manifest not found: $manifestPath – skipping $svcName"
			continue
		}
		$manifestContent = (Get-Content $manifestPath -Raw) -replace 'PLACEHOLDER_IMAGE', $svcImage
		Invoke-KubectlWithInputFile -Args @("apply", "-n", $Namespace) -InputContent $manifestContent

		Write-Host "  [applied] $svcName -> $svcImage (http=$($info.HttpPort) grpc=$($info.GrpcPort))"
	}
}

function Get-ZonesFromJson {
	param([Parameter(Mandatory = $true)][string]$Path)

	if (-not (Test-Path $Path)) {
		throw "zones config not found: $Path. You can copy deploy/k8s/zones.sample.json to this path and edit it."
	}

	$raw = Get-Content -Path $Path -Raw
	$extension = [System.IO.Path]::GetExtension($Path).ToLowerInvariant()
	$parsed = $null

	function Convert-ZonesYamlFallback {
		param([Parameter(Mandatory = $true)][string]$YamlText)

		$zones = @()
		$currentZone = $null

		$lines = $YamlText -split "`r?`n"
		foreach ($line in $lines) {
			$trimmed = $line.Trim()
			if ([string]::IsNullOrWhiteSpace($trimmed)) {
				continue
			}

			if ($trimmed.StartsWith("#")) {
				continue
			}

			if ($trimmed -eq "zones:") {
				continue
			}

			if ($trimmed -match "^-\s*name\s*:\s*(.+)$") {
				if ($null -ne $currentZone) {
					$zones += $currentZone
				}

				$currentZone = [ordered]@{
					name = $matches[1].Trim().Trim('"', "'")
					zoneId = $null
					replicas = [ordered]@{
						centre = $null
						gate = $null
						scene = $null
					}
				}
				continue
			}

			if ($null -eq $currentZone) {
				continue
			}

			if ($trimmed -match "^zoneId\s*:\s*(\d+)$") {
				$currentZone.zoneId = [int]$matches[1]
				continue
			}

			if ($trimmed -match "^(centre|gate|scene)\s*:\s*(\d+)$") {
				$currentZone.replicas[$matches[1]] = [int]$matches[2]
				continue
			}
		}

		if ($null -ne $currentZone) {
			$zones += $currentZone
		}

		return [pscustomobject]@{ zones = $zones }
	}

	switch ($extension) {
		".json" {
			$parsed = $raw | ConvertFrom-Json
		}
		".yaml" {
			$yamlParser = Get-Command ConvertFrom-Yaml -ErrorAction SilentlyContinue
			if ($null -ne $yamlParser) {
				$parsed = $raw | ConvertFrom-Yaml
			}
			else {
				$parsed = Convert-ZonesYamlFallback -YamlText $raw
			}
		}
		".yml" {
			$yamlParser = Get-Command ConvertFrom-Yaml -ErrorAction SilentlyContinue
			if ($null -ne $yamlParser) {
				$parsed = $raw | ConvertFrom-Yaml
			}
			else {
				$parsed = Convert-ZonesYamlFallback -YamlText $raw
			}
		}
		default {
			throw "Unsupported zones config extension '$extension'. Use .json, .yaml, or .yml. path=$Path"
		}
	}

	if ($null -eq $parsed.zones -or $parsed.zones.Count -eq 0) {
		throw "zones config has no zones: $Path"
	}

	$result = @()
	foreach ($zone in $parsed.zones) {
		if ([string]::IsNullOrWhiteSpace($zone.name)) {
			throw "zone name is required in zones config: $Path"
		}

		if ($null -eq $zone.zoneId) {
			throw "zoneId is required for zone '$($zone.name)' in: $Path"
		}

		$zoneCentre = $CentreReplicas
		$zoneGate = $GateReplicas
		$zoneScene = $SceneReplicas

		if ($null -ne $zone.replicas) {
			if ($null -ne $zone.replicas.centre) { $zoneCentre = [int]$zone.replicas.centre }
			if ($null -ne $zone.replicas.gate) { $zoneGate = [int]$zone.replicas.gate }
			if ($null -ne $zone.replicas.scene) { $zoneScene = [int]$zone.replicas.scene }
		}

		$result += [pscustomobject]@{
			name = [string]$zone.name
			zoneId = [int]$zone.zoneId
			centre = $zoneCentre
			gate = $zoneGate
			scene = $zoneScene
		}
	}

	return ,$result
}

function Apply-Zone {
	param(
		[Parameter(Mandatory = $true)][string]$CurrentZoneName,
		[Parameter(Mandatory = $true)][int]$CurrentZoneId,
		[Parameter(Mandatory = $true)][int]$CurrentCentreReplicas,
		[Parameter(Mandatory = $true)][int]$CurrentGateReplicas,
		[Parameter(Mandatory = $true)][int]$CurrentSceneReplicas
	)

	$namespace = Get-ZoneNamespace -Name $CurrentZoneName
	Write-Host "Applying zone deployment: zone=$CurrentZoneName zone_id=$CurrentZoneId namespace=$namespace"
	Write-Host "Ops profile resolved: profile=$OpsProfile gate_service_type=$GateServiceType centre=$CurrentCentreReplicas gate=$CurrentGateReplicas scene=$CurrentSceneReplicas"

	Ensure-Namespace -Namespace $namespace

	$configMapName = "node-config"
	$gateServiceName = "gate-entry"
	$configMapYaml = New-NodeConfigMapYaml -CurrentZoneId $CurrentZoneId -ConfigName $configMapName
	Invoke-KubectlWithInputFile -Args @("apply", "-n", $namespace) -InputContent $configMapYaml

	$gateYaml = New-NodeDeploymentYaml -NodeName "gate" -Replicas $CurrentGateReplicas -RpcPort 18000 -StartCommand "./gate" -ConfigMapName $configMapName
	$sceneYaml = New-NodeDeploymentYaml -NodeName "scene" -Replicas $CurrentSceneReplicas -RpcPort 19000 -StartCommand "./scene" -ConfigMapName $configMapName
	$gateServiceYaml = New-GateServiceYaml -ServiceName $gateServiceName

	Invoke-KubectlWithInputFile -Args @("apply", "-n", $namespace) -InputContent $gateYaml
	Invoke-KubectlWithInputFile -Args @("apply", "-n", $namespace) -InputContent $sceneYaml
	Invoke-KubectlWithInputFile -Args @("apply", "-n", $namespace) -InputContent $gateServiceYaml

	Apply-GoSvcManifests -Namespace $namespace -CurrentZoneId $CurrentZoneId

	Apply-JavaSvcManifests -Namespace $namespace

	Wait-ForZoneReady -Namespace $namespace

	Write-Host "Zone deployment applied: namespace=$namespace"
}

function Remove-Zone {
	param([Parameter(Mandatory = $true)][string]$CurrentZoneName)

	$namespace = Get-ZoneNamespace -Name $CurrentZoneName
	Write-Host "Deleting zone namespace: $namespace"
	Invoke-Kubectl -Args @("delete", "namespace", $namespace) -AllowFailure
}

function Show-ZoneStatus {
	param([Parameter(Mandatory = $true)][string]$CurrentZoneName)

	$namespace = Get-ZoneNamespace -Name $CurrentZoneName
	Write-Host "Zone status for namespace=$namespace"
	Invoke-Kubectl -Args @("get", "deploy,po,svc,cm", "-n", $namespace) -AllowFailure
}

function Apply-Infra {
	Write-Host "Deploying shared infrastructure to namespace $InfraNamespace"
	Write-Host "Kafka profile: $KafkaProfile (broker_retention_ms=$KafkaBrokerRetentionMs db_task_retention_ms=$KafkaDbTaskRetentionMs)"
	Ensure-Namespace -Namespace $InfraNamespace

	foreach ($manifest in @("etcd.yaml", "redis.yaml", "kafka.yaml", "mysql.yaml")) {
		$path = Join-Path $InfraManifestsDir $manifest
		if (-not (Test-Path $path)) {
			Write-Warning "Infra manifest not found: $path — skipping"
			continue
		}
		if ($manifest -eq "kafka.yaml") {
			$manifestContent = Get-Content -Path $path -Raw
			$manifestContent = $manifestContent.Replace("__KAFKA_LOG_RETENTION_MS__", [string]$KafkaBrokerRetentionMs)
			$manifestContent = $manifestContent.Replace("__KAFKA_LOG_RETENTION_CHECK_INTERVAL_MS__", [string]$KafkaRetentionCheckIntervalMs)
			$manifestContent = $manifestContent.Replace("__KAFKA_LOG_RETENTION_BYTES__", [string]$KafkaRetentionBytes)
			$manifestContent = $manifestContent.Replace("__KAFKA_LOG_SEGMENT_BYTES__", [string]$KafkaSegmentBytes)
			$manifestContent = $manifestContent.Replace("__KAFKA_HEAP_OPTS__", [string]$KafkaHeapOpts)
			Invoke-KubectlWithInputFile -Args @("apply", "-n", $InfraNamespace) -InputContent $manifestContent
		}
		else {
			Invoke-Kubectl -Args @("apply", "-n", $InfraNamespace, "-f", $path)
		}
	}

	Write-Host "Shared infrastructure deployed: namespace=$InfraNamespace"
}

function Remove-Infra {
	Write-Host "Deleting shared infrastructure namespace: $InfraNamespace"
	Invoke-Kubectl -Args @("delete", "namespace", $InfraNamespace) -AllowFailure
}

function Show-InfraStatus {
	Write-Host "Shared infrastructure status for namespace=$InfraNamespace"
	Invoke-Kubectl -Args @("get", "deploy,po,svc,cm", "-n", $InfraNamespace) -AllowFailure
}

function Resolve-ZonesConfigPath {
	if (-not [string]::IsNullOrWhiteSpace($ZonesConfigPath)) {
		return $ZonesConfigPath
	}

	return (Join-Path $K8sRoot "zones.json")
}

Ensure-KubectlAvailable
Apply-OpsProfileDefaults
Apply-KafkaProfileDefaults
Show-ExposureProfileWarning

switch ($Command) {
	"infra-up" {
		Apply-Infra
	}
	"infra-down" {
		Remove-Infra
	}
	"infra-status" {
		Show-InfraStatus
	}
	"zone-up" {
		Apply-Zone -CurrentZoneName $ZoneName -CurrentZoneId $ZoneId -CurrentCentreReplicas $CentreReplicas -CurrentGateReplicas $GateReplicas -CurrentSceneReplicas $SceneReplicas
	}
	"zone-down" {
		Remove-Zone -CurrentZoneName $ZoneName
	}
	"zone-status" {
		Show-ZoneStatus -CurrentZoneName $ZoneName
	}
	"all-up" {
		if (-not $SkipInfra) {
			Apply-Infra
		}
		$zonesPath = Resolve-ZonesConfigPath
		$zones = Get-ZonesFromJson -Path $zonesPath
		foreach ($zone in $zones) {
			Apply-Zone -CurrentZoneName $zone.name -CurrentZoneId $zone.zoneId -CurrentCentreReplicas $zone.centre -CurrentGateReplicas $zone.gate -CurrentSceneReplicas $zone.scene
		}
	}
	"all-down" {
		$zonesPath = Resolve-ZonesConfigPath
		$zones = Get-ZonesFromJson -Path $zonesPath
		foreach ($zone in $zones) {
			Remove-Zone -CurrentZoneName $zone.name
		}
		if (-not $SkipInfra) {
			Remove-Infra
		}
	}
	"all-status" {
		Show-InfraStatus
		$zonesPath = Resolve-ZonesConfigPath
		$zones = Get-ZonesFromJson -Path $zonesPath
		foreach ($zone in $zones) {
			Show-ZoneStatus -CurrentZoneName $zone.name
		}
	}
	default {
		throw "Unsupported command: $Command"
	}
}
