# K8s Gate Exposure Guidance

- Gate external exposure rule: managed cloud K8s prefers LoadBalancer; self-hosted / bare metal K8s prefers NodePort plus external L4 load balancer.
- Do not recommend LoadBalancer as a universal default when the cluster lacks a mature LB implementation.
- Prefer explicit OpsProfile choices (`managed-cloud` or `bare-metal`) in examples and operational docs.
- Script baseline default for `custom` profile is `GateServiceType=NodePort`; `managed-cloud` profile still forces `LoadBalancer`.
