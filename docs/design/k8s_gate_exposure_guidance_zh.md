# K8s Gate 暴露方式指南

- Gate 外部暴露规则：托管云 K8s 优先使用 LoadBalancer；自建/裸金属 K8s 优先使用 NodePort 加外部 L4 负载均衡器。
- 当集群没有成熟的 LB 实现时，不要将 LoadBalancer 作为通用默认推荐。
- 在示例和运维文档中，优先使用明确的 OpsProfile 选择（`managed-cloud` 或 `bare-metal`）。
- `custom` 配置的脚本默认基线为 `GateServiceType=NodePort`；`managed-cloud` 配置仍强制使用 `LoadBalancer`。
