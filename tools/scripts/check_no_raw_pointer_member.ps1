param(
    [Parameter(Mandatory = $true)]
    [string]$ProjectDir,

    [Parameter(Mandatory = $false)]
    [string]$ProjectName = "unknown-project"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $ProjectDir -PathType Container)) {
    Write-Error "ProjectDir does not exist: $ProjectDir"
    exit 1
}

$sourceFiles = Get-ChildItem -LiteralPath $ProjectDir -Recurse -File -Include *.h,*.hpp,*.hh,*.hxx,*.cpp,*.cc,*.cxx |
    Where-Object {
        $_.FullName -notmatch "\\(third_party|generated|bin|x64|build|\.vs)\\"
    }

if (-not $sourceFiles) {
    Write-Host "[no-raw-pointer-member] No C/C++ source files found under $ProjectDir"
    exit 0
}

$violations = [System.Collections.Generic.List[object]]::new()

foreach ($file in $sourceFiles) {
    $lines = Get-Content -LiteralPath $file.FullName

    $inBlockComment = $false
    $braceDepth = 0
    $scopeStack = New-Object System.Collections.Stack
    $pendingTypeDecl = $false

    for ($i = 0; $i -lt $lines.Count; $i++) {
        $rawLine = $lines[$i]
        $line = $rawLine

        if ($inBlockComment) {
            if ($line -match "\*/") {
                $line = $line -replace "^.*?\*/", ""
                $inBlockComment = $false
            } else {
                continue
            }
        }

        while ($line -match "/\*") {
            if ($line -match "/\*.*\*/") {
                $line = $line -replace "/\*.*?\*/", ""
            } else {
                $line = $line -replace "/\*.*$", ""
                $inBlockComment = $true
                break
            }
        }

        if ($inBlockComment) {
            continue
        }

        $line = $line -replace "//.*$", ""

        if ([string]::IsNullOrWhiteSpace($line)) {
            continue
        }

        if ($line.TrimStart().StartsWith("#")) {
            continue
        }

        if ($line -match "^\s*(class|struct)\s+[A-Za-z_]\w*\s*;\s*$") {
            # Forward declaration, ignore.
        } elseif ($line -match "\b(class|struct)\s+[A-Za-z_]\w*[^;{]*\{") {
            $scopeStack.Push(@{ depth = $braceDepth + 1 })
            $pendingTypeDecl = $false
        } elseif ($line -match "\b(class|struct)\s+[A-Za-z_]\w*[^;{]*$") {
            # Handle multiline class/struct declaration where '{' is in a later line.
            $pendingTypeDecl = $true
        } elseif ($pendingTypeDecl -and $line -match "\{") {
            $scopeStack.Push(@{ depth = $braceDepth + 1 })
            $pendingTypeDecl = $false
        }

        $insideClassOrStruct = ($scopeStack.Count -gt 0)

        if ($insideClassOrStruct) {
            $trimmed = $line.Trim()

            $isAccessLabel = $trimmed -match "^(public|protected|private)\s*:\s*$"
            $isUsingLike = $trimmed -match "^(using|typedef|friend|static_assert)\b"
            $looksLikeMethod = $line.Contains("(")

            if (-not $isAccessLabel -and -not $isUsingLike -and -not $looksLikeMethod) {
                # Match plain member declarations that contain raw pointer declarators.
                $isPointerField = $line -match "^\s*(?:mutable\s+)?(?:static\s+)?(?:constexpr\s+)?(?:const\s+)?(?:volatile\s+)?[A-Za-z_][\w:<>,\s]*\*\s*[A-Za-z_]\w*(?:\s*\[[^\]]*\])?\s*(?:=\s*[^;]+)?;\s*$"

                if ($isPointerField) {
                    $violations.Add([PSCustomObject]@{
                        File = $file.FullName
                        Line = $i + 1
                        Code = $rawLine.Trim()
                    })
                }
            }
        }

        $openCount = ([regex]::Matches($line, "\\{")).Count
        $closeCount = ([regex]::Matches($line, "\\}")).Count

        $braceDepth += $openCount

        for ($j = 0; $j -lt $closeCount; $j++) {
            while ($scopeStack.Count -gt 0 -and $scopeStack.Peek().depth -eq $braceDepth) {
                [void]$scopeStack.Pop()
            }

            if ($braceDepth -gt 0) {
                $braceDepth--
            }
        }
    }
}

if ($violations.Count -gt 0) {
    Write-Host "[no-raw-pointer-member] FAILED in project '$ProjectName'."
    Write-Host "[no-raw-pointer-member] Raw pointer member variables are prohibited in class/struct declarations:"

    foreach ($v in $violations) {
        Write-Host "  $($v.File):$($v.Line) -> $($v.Code)"
    }

    exit 1
}

Write-Host "[no-raw-pointer-member] PASSED for '$ProjectName'."
exit 0
