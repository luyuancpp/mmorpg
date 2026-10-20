#requires -Version 7
<#
.SYNOPSIS
    邮件 MailAdmin admin 生产者签名密钥(docs/design/mail-system.md §2.3 / §7.2)**本机开发值**的唯一来源。

.DESCRIPTION
    MailAdmin(SendPersonalMail / SendSystemMail)只认 gRPC metadata x-mail-admin-token 与进程环境变量
    MAIL_PRODUCER_SECRET_ADMIN 逐字节一致的调用;变量缺失或去首尾空白后不足 32 字节,mail 把 MailAdmin 整个停用
    (启动横幅 mail_admin=disabled,所有调用回 PermissionDenied)。本机要发信的有三方,必须拿到**同一把**:
      - mail 进程(由 go_services.ps1 的 Start-ServiceInstance 注入,只给 mail);
      - 运维 CLI go/mail/cmd/mailadmin(从同名环境变量读);
      - robot mail-smoke(从同名环境变量读,放进 metadata)。
    所以本机必须有一份跨进程一致的值 —— 这正是本文件存在的理由。

    值不写死在脚本里:令牌不进仓库、不进日志、不进指标、不进错误文本(AGENTS §11.3)。
    值只在本机生成一次,落在 run/secrets/mail-admin-producer-dev.env;`run/*` 在 .gitignore 里,永远不会被提交。
    删掉那个文件会重新生成新值 —— 此时 mail 要重启,正在跑 robot / CLI 的 shell 也要重新取一次令牌。

    生成的值是 64 个十六进制字符,刻意不含空白(Go 侧 strings.TrimSpace 后再比长度,不给口径差异留机会)。

    **仅限本机 dev 的约定**。预发 / 生产的令牌由部署侧从环境变量 MAIL_PRODUCER_SECRET_ADMIN 读入、
    经 Secret mail-admin-auth 注入容器(k8s_deploy.ps1),部署链从不调用本文件的任何函数。

    调用方(tools/scripts/go_services.ps1)的用法:
        . (Join-Path $ScriptDir 'lib/mail_admin_dev_token.ps1')
        $prev = Backup-MailAdminDevToken
        try { Initialize-MailAdminDevToken -RepoRoot $RepoRoot | Out-Null; ...启动 mail... } finally { Restore-MailAdminDevToken $prev }

    在**自己的 shell** 里跑 robot mail-smoke 或 mailadmin CLI 时(go_services 在 finally 里已把变量还原,
    你的 shell 里默认没有令牌),在任意目录 dot-source 本文件再调 Initialize(不传 -RepoRoot):
        . ..\tools\scripts\lib\mail_admin_dev_token.ps1; Initialize-MailAdminDevToken | Out-Null   # 在 robot/ 下;不调 Restore:令牌留在当前会话
    不传 -RepoRoot 时仓库根按本文件自身的位置推出(tools/scripts/lib 往上三级),与当前目录无关 ——
    按当前目录推会在 robot/ 下另生成一把令牌、写进不被 .gitignore 覆盖的 robot/run/secrets/。
    显式传的 -RepoRoot 必须像本仓库的根(有 go/mail/etc/mail.yaml),否则直接报错,不生成、不写文件。
    限制:调用方自己 export 了一个足够长的值时,Initialize 原样保留、不同步进文件(不把调用方自带的令牌悄悄落盘);
    那样的话,起 mail 的 shell 与跑 robot / CLI 的 shell 必须 export 同一个值。
    **取过令牌的 shell 只用来跑 robot / CLI,不要在里面起栈**:令牌留在会话里,从它起的 gate / scene / 各 Go 服务都会继承,
    违背"只给 mail";起 mail 时 Initialize 也会直接沿用会话里的值、不再经文件。起栈用另一个 shell。
#>

Set-StrictMode -Off

# 与 go/mail/internal/config/config.go 的 AdminTokenEnv 逐字一致(Go 侧写死这个名字,ConfigMap / yaml 一个字都不写)。
$script:MailAdminDevTokenEnvName = 'MAIL_PRODUCER_SECRET_ADMIN'

# 与 go/mail/internal/config/config.go 的 MinAdminTokenBytes 同值。去首尾空白后不足这么长视同**未配置**,
# mail 静默停用 MailAdmin —— 所以本机兜底值必须显著长于它。
$script:MailAdminDevTokenMinLength = 32

# 生成 32 字节随机数 → 64 个十六进制字符,远超下限,且不含空白 / 引号,经环境变量、metadata 传递都不会被改写。
$script:MailAdminDevTokenRandomBytes = 32

# 只在一次脚本运行里播报一次,免得多实例启动时刷屏。播报**不回显值**。
$script:MailAdminDevTokenAnnounced = $false

# 默认仓库根:本文件在 tools/scripts/lib 下,往上三级。dot-source 时 $PSScriptRoot 是本文件所在目录,不随调用方的当前目录变。
$script:MailAdminDevTokenDefaultRepoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))

# 认定"这是本仓库根"的标记文件(相对仓库根)。选 mail 的服务配置:令牌只对 mail 有意义,没有它的目录不是起 mail 的仓库。
$script:MailAdminDevTokenRepoMarker = 'go/mail/etc/mail.yaml'

function Get-MailAdminDevTokenStorePath {
    <#
    .SYNOPSIS
        本机开发签名密钥文件的路径(run/secrets/mail-admin-producer-dev.env,已被 .gitignore 的 `run/*` 覆盖)。
    #>
    param([Parameter(Mandatory = $true)][string]$RepoRoot)
    return (Join-Path $RepoRoot 'run/secrets/mail-admin-producer-dev.env')
}

function Assert-MailAdminDevTokenRepoRoot {
    <#
    .SYNOPSIS
        RepoRoot 必须像本仓库的根(有 go/mail/etc/mail.yaml),否则抛错。
    .DESCRIPTION
        `run/*` 的忽略规则只锚定在仓库根:把令牌文件写到别的目录(比如 robot/run/secrets),`git add -A` 就会把它提交进仓库,
        而且生成的是另一把令牌,mail 只会回 PermissionDenied、日志里查不到原因。所以宁可报错,也不在不像仓库根的目录里生成。
        报错文本只含路径,不含令牌值。
    #>
    param([Parameter(Mandatory = $true)][string]$RepoRoot)
    $marker = Join-Path $RepoRoot $script:MailAdminDevTokenRepoMarker
    if (-not (Test-Path -LiteralPath $marker -PathType Leaf)) {
        throw "mail_admin_dev_token: '$RepoRoot' does not look like the repository root ($($script:MailAdminDevTokenRepoMarker) not found); refusing to create run/secrets/mail-admin-producer-dev.env there. Call Initialize-MailAdminDevToken without -RepoRoot to use the repository this library belongs to."
    }
}

function Backup-MailAdminDevToken {
    <#
    .SYNOPSIS
        记下当前进程里 MAIL_PRODUCER_SECRET_ADMIN 的原值,供 Restore 还原。
    .DESCRIPTION
        返回值**包一层哈希表**:单变量若直接返回字符串,"原本没设"($null)就和"没备份过"($null)分不开,
        Restore 会在入口的 $null 判断处直接返回,把注入的令牌留在父进程里。
        与 assetop_dev_secret.ps1 同一条纪律:临时改父进程环境只为让子进程继承,用完必须还原。
    #>
    return @{ Value = [Environment]::GetEnvironmentVariable($script:MailAdminDevTokenEnvName) }
}

function Restore-MailAdminDevToken {
    <#
    .SYNOPSIS
        还原 Backup-MailAdminDevToken 记下的原值,包含"原本没设"这一种情况。
    #>
    param($Saved)
    if ($null -eq $Saved) { return }
    $previous = $Saved.Value
    if ($null -eq $previous) {
        Remove-Item "Env:$($script:MailAdminDevTokenEnvName)" -ErrorAction SilentlyContinue
    } else {
        [Environment]::SetEnvironmentVariable($script:MailAdminDevTokenEnvName, $previous)
    }
}

function New-MailAdminDevTokenValue {
    <#
    .SYNOPSIS
        生成一把本机开发签名密钥(64 个十六进制字符)。用密码学随机源,不是 Get-Random。
    #>
    $bytes = [byte[]]::new($script:MailAdminDevTokenRandomBytes)
    [System.Security.Cryptography.RandomNumberGenerator]::Fill($bytes)
    # 逐字节转十六进制而不是 [Convert]::ToHexString:后者要 .NET 5(理由同 assetop_dev_secret.ps1)。
    return (($bytes | ForEach-Object { $_.ToString('x2') }) -join '')
}

function Read-MailAdminDevTokenStore {
    <#
    .SYNOPSIS
        读 run/secrets/mail-admin-producer-dev.env,返回令牌字符串;文件不在、读不动或值不足 32 字符返回 $null。
    .DESCRIPTION
        只认 `MAIL_PRODUCER_SECRET_ADMIN=<值>` 这一行,`#` 开头是注释。**不做任何回显**:返回值就是令牌,
        调用方除了塞进子进程环境之外不许拿它做别的事。
    #>
    param([Parameter(Mandatory = $true)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { return $null }
    try {
        foreach ($line in (Get-Content -LiteralPath $Path -ErrorAction Stop)) {
            $trimmed = $line.Trim()
            if ($trimmed.Length -eq 0 -or $trimmed.StartsWith('#')) { continue }
            $split = $trimmed.IndexOf('=')
            if ($split -le 0) { continue }
            $name = $trimmed.Substring(0, $split).Trim()
            $value = $trimmed.Substring($split + 1).Trim()
            if ($name -ceq $script:MailAdminDevTokenEnvName -and $value.Length -ge $script:MailAdminDevTokenMinLength) {
                return $value
            }
        }
    } catch {
        # 读不动就当没有:下面会重新生成并覆盖写回。刻意不抛 —— 起服不该被一个本机缓存文件挡住。
        # 异常文本可能含路径,但不会含令牌值。
        return $null
    }
    return $null
}

function Write-MailAdminDevTokenStore {
    <#
    .SYNOPSIS
        把令牌写回 run/secrets/mail-admin-producer-dev.env(整文件重写,自带说明抬头)。
    #>
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Value
    )
    $dir = Split-Path -Parent $Path
    if (-not (Test-Path -LiteralPath $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }

    $lines = New-Object System.Collections.Generic.List[string]
    $lines.Add('# 邮件 MailAdmin 的**本机开发签名密钥**,由 tools/scripts/lib/mail_admin_dev_token.ps1 首次运行时随机生成。')
    $lines.Add('# 仅限本机 dev:预发 / 生产的令牌由部署侧注入(k8s_deploy.ps1 → Secret mail-admin-auth)。')
    $lines.Add('# run/* 在 .gitignore 里,本文件不会进仓库;不要手工拷贝到别的机器或别的环境。')
    $lines.Add('# 删掉本文件会重新生成新值 —— mail 要重启,正在跑 robot mail-smoke / mailadmin CLI 的 shell 也要重新取令牌。')
    $lines.Add("$($script:MailAdminDevTokenEnvName)=$Value")
    # 一次性整文件写入:半截文件会让下一次读到不足 32 字符的值而被当成没有,于是重新生成、与在跑的进程分家。
    [System.IO.File]::WriteAllLines($Path, $lines)
}

function Initialize-MailAdminDevToken {
    <#
    .SYNOPSIS
        保证当前进程环境里有一把 ≥32 字节的 MailAdmin 令牌,供随后启动的子进程(或本 shell 里的 robot / CLI)继承。

    .DESCRIPTION
        取值优先级(与 assetop_dev_secret.ps1 同一条"只在没设时兜底"):
          1. 调用者已经显式设好且足够长的环境变量 —— 原样保留,一个字节都不改,也不碰文件;
          2. run/secrets/mail-admin-producer-dev.env 里已有的值 —— 复用,保证 mail 与 robot / CLI 拿到同一把;
          3. 都没有 —— 随机生成并写入该文件。

        跨进程并发(多 zone 同时起 mail)用命名互斥量串起来:两个进程各生成一把不同的值再互相覆盖,
        会造出"一半进程拿旧值"的静默不一致(表现是 PermissionDenied 且日志里没有令牌线索)。

        **返回值是本次注入的把数(0 或 1),不是令牌值。**

    .PARAMETER RepoRoot
        仓库根目录(令牌文件按它定位)。不传 = 本文件所属的仓库($script:MailAdminDevTokenDefaultRepoRoot),与当前目录无关。
        传了就必须像仓库根(Assert-MailAdminDevTokenRepoRoot),否则报错 —— 先于一切读写,调用方自带令牌时也一样。
    #>
    param(
        [string]$RepoRoot = $script:MailAdminDevTokenDefaultRepoRoot
    )

    Assert-MailAdminDevTokenRepoRoot -RepoRoot $RepoRoot

    $current = [Environment]::GetEnvironmentVariable($script:MailAdminDevTokenEnvName)
    if ($null -ne $current -and $current.Trim().Length -ge $script:MailAdminDevTokenMinLength) { return 0 }

    $path = Get-MailAdminDevTokenStorePath -RepoRoot $RepoRoot
    # 互斥量名里不含任何令牌信息;Global\ 前缀让不同会话的 pwsh 也能互斥。
    # 建不出来(权限受限的环境、非 Windows)就退化成无锁:并发首次生成有极小概率各写各的,重跑一次即可收敛。
    $mutex = $null
    try { $mutex = New-Object System.Threading.Mutex($false, 'Global\mmorpg-mail-admin-dev-token') } catch { $mutex = $null }
    $held = $false
    try {
        if ($null -ne $mutex) {
            try { $held = $mutex.WaitOne(10000) } catch [System.Threading.AbandonedMutexException] { $held = $true }
        }

        $token = Read-MailAdminDevTokenStore -Path $path
        if ($null -eq $token) {
            $token = New-MailAdminDevTokenValue
            Write-MailAdminDevTokenStore -Path $path -Value $token
        }
        [Environment]::SetEnvironmentVariable($script:MailAdminDevTokenEnvName, $token)
    } finally {
        if ($held) { $mutex.ReleaseMutex() }
        if ($null -ne $mutex) { $mutex.Dispose() }
    }

    if (-not $script:MailAdminDevTokenAnnounced) {
        $script:MailAdminDevTokenAnnounced = $true
        # 只说出处,**不回显值**;相对路径让人知道去哪删,不至于以为它进了仓库。
        Write-Host "[mail] Injected local MailAdmin admin producer development secret (value hidden; loaded from run/secrets/mail-admin-producer-dev.env; local dev only)" -ForegroundColor DarkGray
    }
    return 1
}
