# Script Directory Rules

- Script placement rule: `tools/scripts` is the canonical home for maintained repo scripts.
- `tools/scripts/third_party` is the canonical home for third-party build/maintenance scripts.
- Top-level `scripts/` is reserved for bootstrap or thin forwarding entrypoints only; do not duplicate implementation there.
- Do not place project-maintained scripts inside vendored trees like `third_party/*` unless they are clearly upstream-owned.
- Current example: gRPC build lives in `tools/scripts/third_party/build_grpc.ps1`, with thin wrapper `scripts/build_grpc.ps1`.
