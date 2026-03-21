#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

if ! command -v pwsh >/dev/null 2>&1; then
  echo "pwsh is required. Install PowerShell first." >&2
  echo "Ubuntu: sudo apt install powershell" >&2
  exit 1
fi

exec pwsh -NoProfile -File "$SCRIPT_DIR/iwyu_run.ps1" "$@"
