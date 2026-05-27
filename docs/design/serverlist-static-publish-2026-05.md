# Server-List 静态发布架构 (Static Publish via OSS + CDN) — 2026-05

**Date:** 2026-05-23
**Status:** Design — 待 Phase 1/2 落地
**关联:**
- [java-gateway-portal-decision.md](./java-gateway-portal-decision.md)(Java Gateway 选型)
- [open-server-rate-limit-design.md](./open-server-rate-limit-design.md)(Bucket4j 限流)
- [login-queue-2026-05.md](./login-queue-2026-05.md)(AssignGate 真排队)
- [architecture-current-state-vs-gaps-2026-05.md](./architecture-current-state-vs-gaps-2026-05.md) 拟新增 #15

> **TL;DR**:把 `/api/server-list` 从 Java Gateway 的"动态接口"改造成"OSS + CDN 的静态资源",
> 玩家流量从 MySQL 解耦,源站 QPS 从 N 万降到 ~0,Gateway 只保留**降级兜底**通道。
> 客户端三档模式(file / gateway / cdn)解决本地调试 → 联调 → 生产的全链路。

---

## 1. 为什么必须做这件事

### 1.1 现状问题(代码定位)

| 文件 | 行号 | 问题 |
|---|---|---|
| `java/gateway_node/.../service/ServerListService.java` | 25-31 | 每次请求 `zoneConfigRepo.findAll()`,**裸打 MySQL,无缓存** |
| `java/gateway_node/.../controller/ServerListController.java` | 19-22 | Controller 无限流(对比 `assign-gate` 已加 Bucket4j) |
| `client/unity/.../GatewayHttpClient.cs` | 76 | Unity 客户端硬编码打 Java Gateway |
| `robot/gate.go` | 73 | robot 同样硬编码打 Java Gateway |

### 1.2 业界做法(米哈游 / 腾讯天美 / 网易 / 莉莉丝)

```
客户端启动
  └─► 读包内 bootstrap URL(写死域名)
       └─► CDN 边缘节点(99.9% 命中)
            └─► 回源 OSS/COS(对象存储,不是业务服务器)
                 ↑
                 │ 运维改 zone 状态时一次性写入
                 │
              [业务侧:MySQL → 发布任务 → OSS]
```

关键差异:**玩家读路径完全不经过业务服务器,源站 QPS ≈ 0**。

### 1.3 不做的代价

- **开服必崩**:5w~50w 客户端在开服那一秒同时拉 server-list,MySQL 连接池打爆,Gateway OOM
- **故障域绑定**:Gateway 任何抖动都让玩家看不到服务器列表 → 公关事故
- **跟 #10 #14 抢资源**:Bucket4j + AssignGate 真排队都在 Gateway 上,server-list 这种本不该动态的接口挤掉它们的容量

---

## 2. 拍板的决策(2026-05-23)

| 决策点 | 选择 | 理由 |
|---|---|---|
| 对象存储 | **阿里云 OSS** | 国内 MMO 主流;米哈游/莉莉丝在用;文档最全 |
| 本地开发模拟 | **MinIO**(S3 协议兼容) | OSS 走 aliyun-oss-sdk,但本地用 S3-compatible 接口的 MinIO,SDK 同源切换 |
| CDN | 阿里云 CDN(主)+ 腾讯云 CDN(备) | 主从异厂,防单家区域故障 |
| 发布触发 | **手动按钮 + 自动触发都要** | 自动:`zone_config` 变更后异步发布;手动:运维"立即发布"按钮兜底 |
| 客户端兜底 | **三档**:CDN 主 → CDN 备 → Gateway 直连 | CDN 全挂时让 Gateway `/api/server-list` 顶住(加 Caffeine + Bucket4j) |
| Gateway `/api/server-list` 命运 | **保留作为降级通道** | 平时 QPS=0,加 30s Caffeine 缓存 + 限流,故障时承担降级流量 |
| zone 数据存哪 | **MySQL `zone_config`(不变)** | 运维数据;不进 Excel(运营数据不能版本化) |

---

## 3. 总体拓扑

```
┌──────────────────── 玩家读路径(99.9%+ 流量)──────────────────┐
│                                                                  │
│  Client (Unity/robot)                                            │
│    │                                                             │
│    │ ① GET https://serverlist-cn1.yourgame.com/serverlist.json  │
│    ▼                                                             │
│  阿里云 CDN(主)                                                │
│    │ miss?                                                       │
│    ▼                                                             │
│  阿里云 OSS bucket: serverlist/cn/serverlist.json                │
│                                                                  │
│  失败 → ② 备用: https://serverlist-cn2.yourgame.com (腾讯云 CDN)│
│  失败 → ③ 兜底: GET {gateway}/api/server-list (Gateway 降级)    │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘

┌──────────────────── 运维写路径(QPS ~0)────────────────────────┐
│                                                                  │
│  GM 后台改 zone_config                                           │
│    │                                                             │
│    │ ① 自动:JPA @PostUpdate / Spring Event                     │
│    │ ② 手动:POST /admin/serverlist/publish                      │
│    ▼                                                             │
│  ServerListPublisher (Java Gateway 内异步任务)                   │
│    │                                                             │
│    │ 1. 查 zone_config 全表 + ZoneHealthProbeService 当前状态   │
│    │ 2. 生成 serverlist.json (schema 见 §4)                      │
│    │ 3. 计算 sha256,作为 ETag                                    │
│    │ 4. 上传 OSS:serverlist.json + serverlist_v{ts}.json         │
│    │    (前者是当前版本,后者归档便于回滚)                       │
│    │ 5. 调 CDN purge API(阿里云 + 腾讯云)                       │
│    │ 6. 写一条审计日志:{operator, ts, zone_count, sha256}        │
│    ▼                                                             │
│  完成,30s 内全网 CDN 生效                                       │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

---

## 4. serverlist.json Schema(契约,B/C 都按这个走)

```json
{
  "schema_version": 1,
  "published_at": 1716451200,
  "publisher": "admin@xxx",
  "etag": "sha256:abc123...",
  "zones": [
    {
      "zone_id": 1,
      "name": "天涯客栈",
      "status": "OPEN",
      "load_level": "SMOOTH",
      "recommended": true,
      "is_new": false,
      "open_time": 0,
      "maintenance_msg": "",
      "sort_order": 1
    },
    {
      "zone_id": 2,
      "name": "竹影山庄",
      "status": "MAINTENANCE",
      "load_level": null,
      "recommended": false,
      "is_new": true,
      "open_time": 0,
      "maintenance_msg": "维护至 14:00",
      "sort_order": 2
    }
  ],
  "fallback_gateway": "https://gateway.yourgame.com"
}
```

字段语义跟 `ServerListResponse` + `ZoneInfoDto`(已存在)一一对应,新增:

- `schema_version`:**强制**,客户端识别不到的版本必须降级到 Gateway 直连
- `published_at` / `publisher` / `etag`:审计 + 客户端 If-None-Match 用
- `fallback_gateway`:CDN 文件里直接告诉客户端兜底地址(防止客户端配置过时)

**强约束**:`status` 取值与 `ZoneDisplayStatus` enum 一致(OPEN / MAINTENANCE / CLOSED / PREVIEW),`load_level` 与 `LoadLevel` 一致(SMOOTH / BUSY / FULL / null)。

---

## 5. Phase 1: Java Gateway 发布机制(B 任务范围)

### 5.1 新增组件

| 组件 | 路径 | 职责 |
|---|---|---|
| `ServerListPublisher` | `service/publisher/ServerListPublisher.java` | 生成 json + 上传 OSS + 触发 CDN purge |
| `OssClient` | `infra/oss/OssClient.java` | 阿里云 OSS 上传抽象;本地切 MinIO |
| `CdnPurgeClient` | `infra/cdn/CdnPurgeClient.java` | 调阿里云 + 腾讯云 purge API |
| `AdminServerListController` | `controller/admin/AdminServerListController.java` | `POST /admin/serverlist/publish`(SaToken 鉴权) |
| `ZoneConfigChangeListener` | `service/publisher/ZoneConfigChangeListener.java` | 监听 `ZoneConfig` JPA 变更事件,异步触发发布 |
| `PublishAuditLog` entity + repo | `entity/PublishAuditLog.java` | 审计 |

### 5.2 配置(application.yml 新增段)

```yaml
serverlist:
  publish:
    enabled: true
    auto-on-zone-change: true       # zone_config 改后自动发布
    debounce-ms: 5000                # 5s 内多次改动合并一次发布
  oss:
    endpoint: "http://minio.local:9000"   # 本地用 MinIO
    bucket: "serverlist-dev"
    access-key: "${OSS_AK}"
    secret-key: "${OSS_SK}"
    object-key: "cn/serverlist.json"
  cdn:
    aliyun:
      enabled: false                  # 本地关
      access-key: "${CDN_ALI_AK}"
      secret-key: "${CDN_ALI_SK}"
      domain: "serverlist-cn1.yourgame.com"
    tencent:
      enabled: false
      secret-id: "${CDN_TX_SID}"
      secret-key: "${CDN_TX_SK}"
      domain: "serverlist-cn2.yourgame.com"
```

### 5.3 Gateway 降级路径改造(`/api/server-list`)

保留接口,但改造:

1. 加 `@Cacheable("serverList")` Caffeine 缓存,TTL 30s
2. 加 Bucket4j 限流(沿用 `open-server-rate-limit-design.md` 模板,zone 维度 1000/s)
3. response header 加 `Cache-Control: public, max-age=15, s-maxage=30`
4. 支持 `If-None-Match` → 304

平时玩家不打这个接口(走 CDN);CDN 全挂时,这是最后一道防线。

### 5.4 OSS 抽象(本地 MinIO ↔ 生产 阿里云 OSS)

```java
public interface OssClient {
    void putObject(String key, byte[] content, String contentType, Map<String,String> metadata);
    boolean exists(String key);
}
```

- 生产:`AliyunOssClient`(用 `aliyun-sdk-oss` 3.x)
- 本地:`S3OssClient`(用 `aws-java-sdk-s3` 指向 MinIO,S3 协议兼容)
- Spring `@ConditionalOnProperty` 切换

**为啥这样切**:阿里云 OSS SDK 不能直连 MinIO,但 MinIO 完整实现 S3 协议;反过来 aws-sdk 也能打阿里云 OSS(它兼容 S3)—— 但生产用阿里云 SDK 性能/特性更好。所以两份实现,各取所长。

### 5.5 CDN purge 失败处理

- purge 是 **best-effort**,失败不阻塞发布
- 失败写审计日志 + 告警
- 客户端会带 `If-None-Match`,即使 CDN 没 purge,30s TTL 后自然刷新

---

## 6. Phase 2: 客户端三档模式(C 任务范围)

### 6.1 robot 改造

`robot/config/config.go` 新增字段:

```go
type Config struct {
    // ... existing fields ...

    // ServerList 拉取模式: "file" / "gateway" / "cdn"
    // file:    本地静态文件(开发机)
    // gateway: 直连 Java Gateway /api/server-list(联调,默认)
    // cdn:     按 ServerListURLs 顺序尝试 CDN(生产/压测)
    ServerListMode string   `yaml:"serverlist_mode"`
    ServerListPath string   `yaml:"serverlist_path"`  // file 模式用
    ServerListURLs []string `yaml:"serverlist_urls"`  // cdn 模式用,按序兜底
}
```

`robot/gate.go` 的 `resolveZoneID()` 重构成 strategy:

```go
func resolveZoneID(cfg *Config) (uint32, error) {
    switch cfg.ServerListMode {
    case "file":
        return resolveFromFile(cfg.ServerListPath)
    case "cdn":
        return resolveFromCDN(cfg.ServerListURLs, cfg.GatewayAddr) // 三档兜底
    case "gateway", "":
        return resolveFromGateway(cfg.GatewayAddr) // 现有逻辑
    default:
        return 0, fmt.Errorf("unknown serverlist_mode: %s", cfg.ServerListMode)
    }
}

func resolveFromCDN(urls []string, gatewayFallback string) (uint32, error) {
    for _, url := range urls {
        if zoneID, err := tryFetch(url); err == nil {
            return zoneID, nil
        }
    }
    // 三档:CDN 全挂,降级 Gateway
    return resolveFromGateway(gatewayFallback)
}
```

### 6.2 Unity 客户端改造

`client/unity/.../Net/GatewayHttpClient.cs` → 拆出 `ServerListClient`,同样三档逻辑;`MmorpgClient.Net` 命名空间。

新增配置(打包配置 / PlayerPrefs / 启动参数):

```csharp
public class ServerListConfig {
    public string mode;              // "file" / "gateway" / "cdn"
    public string filePath;
    public string[] cdnUrls;
    public string gatewayUrl;
}
```

**调试技巧**:
- 开发机:`run/dev/serverlist.json` 提交一份样例进 git,新人 clone 即用
- 联调:切 `mode=gateway`,直连 `http://127.0.0.1:8080`,实时反映 MySQL
- 内网压测:`mode=cdn` + 内网 MinIO 模拟 CDN

### 6.3 客户端缓存策略

- 内存缓存:启动后保留 10min(避免来回切服面板时反复拉)
- 磁盘缓存:写一份 `~/.../serverlist_cached.json`,启动时先用���存渲染列表,再异步刷新(冷启动体感)
- ETag:记住上次 `etag` header,下次请求带 `If-None-Match`,304 直接用缓存

---

## 7. 故障场景演练表

| 场景 | 玩家体感 | 兜底机制 |
|---|---|---|
| 主 CDN 区域故障 | 无感(自动切备 CDN) | 客户端三档第二档 |
| 主 + 备 CDN 全挂 | 1 秒延迟切到 Gateway | 客户端三档第三档(`fallback_gateway`) |
| OSS 故障 + CDN 缓存未过期 | 无感 | CDN 30s TTL 内继续返回旧数据 |
| OSS 故障 + CDN 缓存过期 | CDN miss 后回源失败 → 客户端切 Gateway | 同上 |
| Gateway + CDN 全挂 | 客户端用磁盘缓存渲染 + 红字"网络异常" | 客户端磁盘缓存 |
| 发布脚本写脏 json | CDN 推全网,玩家看到错误数据 | 审计日志 + 一键回滚到 `serverlist_v{ts-1}.json` |

---

## 8. 安全考量

- `/admin/serverlist/publish`:SaToken 强鉴权,仅运维角色
- OSS 上传用 RAM 子账号,`PutObject` 权限只允许特定 bucket + 前缀
- json 内不含敏感字段(IP/端口都没有,只有 zone 元数据)
- CDN 域名走 HTTPS,防中间人篡改

---

## 9. 落地步骤

| 阶段 | 内容 | 预计 | 谁来 |
|---|---|---|---|
| **A** | 本设计文档(本文) | 0.5d | 已完成 |
| **B** | Java Gateway 发布机制(§5) | 1d | TBD |
| **C** | 客户端三档模式(§6) | 0.5d | TBD |
| **D** | 联调:本地 MinIO + 三档全跑通 | 0.5d | TBD |
| **E** | 上预发:阿里云 OSS + CDN 真链路 | 0.5d | 运维 |
| **F** | 压测:模拟 5w 客户端冷启动 | 0.5d | 接 [stress-test runbook](../ops/login-queue-stress-runbook.md) |

**Phase 1(B)和 Phase 2(C)契约由本文 §4 锁定,可并行**。

---

## 10. 待办 / 后续优化

1. **多语言 / 多区域支持**:`serverlist/{cn,sea,na,eu}/serverlist.json`,客户端按 region 选 key
2. **灰度发布**:支持发布到 `serverlist_canary.json`,运维内部 IP 命中,验证后再切正式
3. **客户端预热**:启动 splash 时异步预拉,登录界面零等待
4. **告警**:OSS 上传失败 / CDN purge 失败 / 客户端走第三档比例 > 1% 都要告警
5. **自动回滚**:如果发布后 1min 内"客户端 fetch 失败率 > 阈值",自动 rollback 到上一版

---

## 11. 关联到 ARCH.md §11 决策表

发布后在 ARCH.md §11 加一行:

| 决策 | 选择 | 文档 |
|---|---|---|
| Server-list 分发模式 | OSS + CDN 静态发布,Gateway 仅做降级 | [serverlist-static-publish-2026-05.md](./serverlist-static-publish-2026-05.md) |

并在 `architecture-current-state-vs-gaps-2026-05.md` 加 #15:

> **#15** Server-List 静态发布(OSS + CDN)— 见本文 §9 落地步骤

---

## 12. FAQ

**Q: zone 数据为什么不进 Excel?**
A: zone 是**运营数据**(运维随时改、不重启、最终一致即可),Excel 是**策划数据**(版本发版时一起出、强一致)。塞进 Excel 意味着开新服要发版,合服要等下个版本 —— 倒退。详见 §2 决策表。

**Q: 为什么不用 Redis 缓存解决?**
A: Redis 缓存 Gateway 还是要扛连接 + 序列化 + 网络栈,5w QPS 单机扛不住,要做 Redis 集群 + Gateway 集群 —— 复杂度爆炸。CDN 边缘节点直接返回字节流,**问题维度不一样**。

**Q: CDN 缓存 30s,运维改 zone 状态延迟怎么办?**
A: 30s 对玩家无感。维护中那 30s 玩家看到 OPEN 状态点进去 → `assign-gate` 会拒(zone manual_status=MAINTENANCE),体验完全可接受。要更快可调小 TTL 或主动 purge。

**Q: 跟 #14 (AssignGate 真排队)冲突吗?**
A: 完全互补。本文解决"看服务器列表"的扛压;#14 解决"点进入服务器后排队"的体验。两条链路,两个洪峰阶段。

**Q: Centre 已退役,跟本文有关吗?**
A: 无关。Centre 退役在登录链路里;server-list 是登录前的"服务器选择"页,从来没经过 Centre。
