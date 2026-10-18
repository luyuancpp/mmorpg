// route_tool 是映射持久化的运维工具(docs/design/player-route-mapping-durability-proposal.md §7、§11.2;
// 运维步骤见 docs/ops/merge-zone-runbook.md §9.3 / §9.4)。
//
// 2026-10-18(第 1b-1 批)起这里只是一个薄包装:逻辑迁到 internal/routetoolcli(参数、装配、退出码)与 internal/routetool
// (各子命令),并且并进了 data_service 主程序 —— K8s 镜像只构建单一入口 /app/service(deploy/k8s/Dockerfile.go-svc),
// 生产上用
//
//	data_service -f <data_service.yaml> -route-tool <子命令> [子命令参数]
//
// 本 cmd 只为 dev 的 `go run ./cmd/route_tool …` 与既有 runbook 命令保留,命令行与输出逐字相同:
//
//	route_tool -f <data_service.yaml> <import-from-redis | audit | tidb-prepare | freeze | reconcile-from-cache | unfreeze> [参数]
//
// 子命令、参数与退出码见 routetoolcli 的包注释与 usage。
package main

import (
	"context"
	"os"

	"data_service/internal/routetoolcli"
)

func main() {
	os.Exit(routetoolcli.Run(context.Background(), os.Args[1:], os.Stdout, os.Stderr))
}
