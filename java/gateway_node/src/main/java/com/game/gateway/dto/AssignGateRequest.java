package com.game.gateway.dto;

public class AssignGateRequest {
    /**
     * Zone the player wants to enter. 0 means "any" — server picks the
     * least-loaded zone. Stored as int because the gRPC contract is
     * uint32; widening to long here just requires a cast on every send.
     */
    private int zoneId;

    /** Optional account hint, used by the limiter for cooldown keying
     *  AND by go-zero for reconnect-bypass detection.
     *  带 queue_token 的重入不把 account 交给限流器做冷却(G4b-J0),account 仍原样转给 login。 */
    private String account;

    /** Stable client device identifier. Threaded through to the queue
     *  meta so ops can audit "which device is queue position #N". */
    private String deviceId;

    /**
     * Re-entry token for clients already in the queue. Empty/null on the
     * first AssignGate call; populated by the response of an earlier
     * AssignGate (or QueryQueueStatus) when the zone was over capacity.
     *
     * Including this in the request lets a flaky client recover after a
     * dropped /api/queue-status poll: hitting AssignGate with the saved
     * token resumes the in-flight queue entry rather than enqueueing
     * twice (which would push the player further back in line).
     *
     * 客户端契约(负责人 2026-10-15 定):持有 queue_token 时一律轮询 /api/queue-status 查排位、取放行票,
     * 该端点不过限流器。
     * - 轮询间隔:每次轮询前等待 = 基础间隔 + 抖动。基础间隔 = max(retry_after_ms, 下限),抖动在 [0, 基础间隔的 20%]
     *   内均匀取。下限平时为 2s;/api/queue-status 连续失败 n 次时为 2s*2^(n-1),封顶 30s(不超过 AdmitTTL 60s 的一半)。
     *   /api/queue-status 回一次非失败回包即把 n 清零,恢复重入的回包不清零。
     * - 「/api/queue-status 本身失败」只指三类:网络错误、HTTP 非 200、回包 code=500。只有这时才带同一 token 重调一次
     *   assign-gate 作恢复。回 503(zone_maintenance / zone_closed / zone_not_open)或 404 zone_not_found 是终态:
     *   不重入,按错误结束。
     * - G4b-J0 之后恢复重入照样过波次、zone 桶和 IP 桶:被波次或 zone 桶挡住时回 code=100、queue_source="ratelimit",
     *   且不带新 token;IP 桶耗尽时回 code=429(IP_RATE_LIMIT)。这两种情况,以及重入回 code=100、queue_source="login"
     *   (续上原排队条目),客户端都保留 token、按上面的间隔回到 /api/queue-status 轮询。不要当成排位丢失,也不要按 429 的
     *   30/60/120s 退避重调 assign-gate:退避会让已被放行的玩家错过 login 的放行窗口(AdmitTTL)。恢复重入本身也失败时
     *   按错误结束,交外层。
     * - 任一端点回 410 时才丢掉 token、不带 token 重调 assign-gate。排队预算耗尽即停,不再发请求。
     * 服务端回包格式不变:两个端点的业务回包一律 HTTP 200 + JSON code。
     * 出处:docs/design/open-server-rate-limit-design.md §11.5 的 2026-10-15 更正与补注。
     */
    private String queueToken;

    public int getZoneId() { return zoneId; }
    public void setZoneId(int zoneId) { this.zoneId = zoneId; }

    public String getAccount() { return account; }
    public void setAccount(String account) { this.account = account; }

    public String getDeviceId() { return deviceId; }
    public void setDeviceId(String deviceId) { this.deviceId = deviceId; }

    public String getQueueToken() { return queueToken; }
    public void setQueueToken(String queueToken) { this.queueToken = queueToken; }
}
