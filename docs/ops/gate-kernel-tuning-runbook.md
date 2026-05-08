# Gate 内核调优 Runbook

**Date:** 2026-05-08
**适用:** 所有 cpp gate 节点机器(裸机 / VM / K8s 节点)
**前提:** Linux 4.12+

---

## 一、为什么要调

压测出现下面任意一条 → 你撞上了 ephemeral port / TIME_WAIT 墙:

- `ss -s` 看到 TIME_WAIT 几万,`Bound` 千级别堆积
- `nstat | grep -i drop` 出现 `TcpExtListenDrops` 增长
- 登录长尾(p99 / max) 从几十 ms 跳到秒级
- 客户端日志: `connect: cannot assign requested address`(EADDRNOTAVAIL)

**实测对比(扩 port + tw_reuse 后)**

| 指标 | 优化前 | 优化后 |
|---|---|---|
| login_fail | 3% | 0 |
| avg_login | 414-453ms | 77-79ms |
| max_login | 18-25s | 0.9-1.2s |
| SynSent | 1540 | 0 |
| Bound | 15470 | 2704 |

---

## 二、固化 sysctl(必做)

写入 `/etc/sysctl.d/99-gate.conf`:

```ini
# === ephemeral port 扩容 ===
net.ipv4.ip_local_port_range     = 1024 65535
# 如有固定监听端口冲突,在此排除
net.ipv4.ip_local_reserved_ports = 3306,6379,8080,9000

# === TIME_WAIT 复用 + 容量 ===
# 主动 connect 端复用 TIME_WAIT 端口(基于 PAWS,安全)
net.ipv4.tcp_tw_reuse            = 1
net.ipv4.tcp_max_tw_buckets      = 1048576
net.ipv4.tcp_fin_timeout         = 15
net.ipv4.tcp_timestamps          = 1   # tw_reuse 依赖

# === 半连接 + 全连接队列 ===
net.core.somaxconn               = 65535
net.ipv4.tcp_max_syn_backlog     = 65535
net.ipv4.tcp_syncookies          = 1

# === 连接 buffer ===
net.core.rmem_max                = 16777216
net.core.wmem_max                = 16777216
net.ipv4.tcp_rmem                = 4096 87380 16777216
net.ipv4.tcp_wmem                = 4096 65536 16777216

# === conntrack(如启用 nf) ===
net.netfilter.nf_conntrack_max   = 1048576

# === keepalive(防 NAT 踢长连) ===
net.ipv4.tcp_keepalive_time      = 300
net.ipv4.tcp_keepalive_intvl     = 30
net.ipv4.tcp_keepalive_probes    = 3
```

加载:
```bash
sudo sysctl --system
sudo sysctl -p /etc/sysctl.d/99-gate.conf
```

⚠️ **绝对不要开** `net.ipv4.tcp_tw_recycle`:
- NAT 环境(K8s / 公网)会丢连接
- 4.12+ 内核已删除该参数

---

## 三、进程级 ulimit

`/etc/security/limits.d/gate.conf`:

```
*  soft  nofile  1048576
*  hard  nofile  1048576
*  soft  nproc   65535
*  hard  nproc   65535
```

systemd 单元 `[Service]`:
```
LimitNOFILE=1048576
LimitNPROC=65535
```

K8s pod:
```yaml
securityContext:
  sysctls:
    - name: net.core.somaxconn
      value: "65535"
```

---

## 四、多源 IP(单 IP 端口耗尽时)

ephemeral port 上限是 **(srcIP, dstIP, dstPort)** 维度的。单 IP 对单后端理论 64K。

加副 IP:
```bash
ip addr add 10.0.0.11/24 dev eth0
ip addr add 10.0.0.12/24 dev eth0
ip addr add 10.0.0.13/24 dev eth0

# 持久化(Ubuntu netplan / RHEL nmcli)
```

让 gate 用上多源 IP(零代码改动):
- 跑多 gate 进程 + `ip rule` 绑不同源 IP
- 或多开 gate 容器/VM 直接横向扩

---

## 五、故障定位 SOP

### 5.1 一条命令判断"谁在短连"

```bash
ss -tan state time-wait | awk '{print $4}' | sort | uniq -c | sort -rn | head
```

| TIME_WAIT 集中在 | 含义 | 修法 |
|---|---|---|
| `<login_ip>:<grpc_port>` | gate→login 是短连接 | 任务 #13 核查 channel 单例 |
| 自己监听端口 | 客户端/压测短连进来 | 客户端开 keep-alive |
| 压测机几万 TIME_WAIT | 是压测机的极限,**不是 gate 的** | 压测机也调 sysctl |

### 5.2 看 accept queue 是否溢出

```bash
ss -lnt              # 看 Recv-Q / Send-Q
nstat | grep -E 'ListenOverflows|ListenDrops|TCPSynRetrans'
```

`ListenOverflows` 增长 → backlog 不够,调 `somaxconn` + `tcp_max_syn_backlog`,并确保业务 `listen(fd, N)` 的 N 够大。

### 5.3 看是不是软中断单核打满

```bash
mpstat -P ALL 1
# %soft 一个核 100% → 网卡 RSS / 多队列没开
ethtool -l eth0
ethtool -L eth0 combined <核数>
```

### 5.4 拐点判断

加压时同时盯:p99 延迟 / CPU% / `ss -s` / `ListenOverflows`

- 延迟陡升 + accept queue 溢出 + CPU 没满 → **服务端真极限**(优化业务 / 扩 worker)
- 延迟陡升 + Bound + SynSent 高 + CPU 没满 → **客户端 ephemeral port 极限**(回到 §二)
- 延迟陡升 + CPU 满 → **算力极限**(横向扩 gate)

---

## 六、阶梯压测建议

```
5k → 10k → 15k → 20k → 30k → 45k → 60k → 80k → 100k
每档 3-5 分钟,记录:
  QPS / avg / p99 / max
  ss -s 各状态计数
  CPU% (us/sy/si)
  网络带宽 / pps
画曲线找拐点。
```

---

## 七、回滚

任何 sysctl 改动想回滚:
```bash
sudo rm /etc/sysctl.d/99-gate.conf
sudo sysctl --system
```

不需要重启机器,内核参数立即生效。

---

## 关联文档
- [docs/design/ARCH.md](../design/ARCH.md) §9 总体调优
- [docs/design/architecture-current-state-vs-gaps-2026-05.md](../design/architecture-current-state-vs-gaps-2026-05.md) 缺口清单
- [docs/design/gate-load-balancing-design.md](../design/gate-load-balancing-design.md) gate 有状态约束
