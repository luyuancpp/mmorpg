package com.game.mall;

import java.util.concurrent.atomic.AtomicInteger;
import java.nio.charset.StandardCharsets;
import org.springframework.dao.DuplicateKeyException;
import org.springframework.context.annotation.DependsOn;
import org.springframework.jdbc.core.JdbcTemplate;
import org.springframework.stereotype.Repository;
import org.springframework.transaction.PlatformTransactionManager;
import org.springframework.transaction.TransactionDefinition;
import org.springframework.transaction.support.TransactionTemplate;

@Repository
@DependsOn("commerceSchemaReady")
public class NonceStore {
    private final JdbcTemplate jdbc;
    private final String nowSql;
    private final TransactionTemplate tx;
    private final AtomicInteger requests = new AtomicInteger();
    public NonceStore(JdbcTemplate jdbc, CommerceProperties props, PlatformTransactionManager manager) {
        this.jdbc = jdbc;
        tx = new TransactionTemplate(manager); tx.setIsolationLevel(TransactionDefinition.ISOLATION_READ_COMMITTED);
        tx.setPropagationBehavior(TransactionDefinition.PROPAGATION_REQUIRES_NEW); tx.setTimeout(5);
        boolean h2 = Boolean.TRUE.equals(jdbc.execute((org.springframework.jdbc.core.ConnectionCallback<Boolean>) connection -> connection.getMetaData().getDatabaseProductName().equals("H2")));
        nowSql = h2 ? "CAST(EXTRACT(EPOCH FROM CURRENT_TIMESTAMP())*1000 AS BIGINT)" : props.isRequireTidb() ? "(TIDB_CURRENT_TSO() >> 18)" : "CAST(UNIX_TIMESTAMP(CURRENT_TIMESTAMP(3))*1000 AS SIGNED)";
        jdbc.setQueryTimeout(5);
    }
    public void claim(String role, String nonce, long sentMs) {
        tx.executeWithoutResult(status -> claimInTransaction(role, nonce, sentMs));
    }
    private void claimInTransaction(String role, String nonce, long sentMs) {
        // TiDB 的 setAutoCommit(false) 是 lazy BEGIN；无真实表访问时 CURRENT_TSO() 仍为 0。
        // 对本请求 nonce 做不加锁主键点读，初始化当前事务，不制造全服共享热锁行。
        jdbc.queryForList("SELECT nonce FROM commerce_request_nonce WHERE caller_role=? AND nonce=?", role, nonce.getBytes(StandardCharsets.US_ASCII));
        long nowMs = jdbc.queryForObject("SELECT " + nowSql, Long.class);
        if (nowMs < HmacAuthentication.WINDOW_MS) { throw new CommerceException(503, "权威认证时间不可用"); }
        // nonce 属于同一个权威 SQL 域；多副本与重启不丢去重信息。清理只处理已过签名窗口的行。
        if ((requests.getAndIncrement() & 63) == 0) {
            for (Expired row : jdbc.query("SELECT caller_role,nonce FROM commerce_request_nonce WHERE expires_ms<? ORDER BY expires_ms,caller_role,nonce LIMIT 128",
                    (rs, index) -> new Expired(rs.getString(1), rs.getBytes(2)), nowMs)) {
                jdbc.update("DELETE FROM commerce_request_nonce WHERE caller_role=? AND nonce=? AND expires_ms<?", row.role(), row.nonce(), nowMs);
            }
        }
        // 时窗由同一权威 SQL 时钟在插入语句内裁决，不能用各应用副本的墙钟各自验过期。
        // 去重再保留 10 分钟，覆盖至多 5 秒的 SQL/行锁等待；禁止暂停旧验签结果后重新插入。
        try {
            int changed = jdbc.update("INSERT INTO commerce_request_nonce(caller_role,nonce,expires_ms) SELECT ?,?,? WHERE ?>=" + nowSql + "-120000 AND ?<=" + nowSql + "+120000",
                    role, nonce.getBytes(StandardCharsets.US_ASCII), sentMs + 720_000, sentMs, sentMs);
            if (changed != 1) { throw new CommerceException(401, "签名请求已过期"); }
        }
        catch (DuplicateKeyException replay) { throw new CommerceException(409, "签名请求已使用，请以新 nonce 重试原业务请求号"); }
    }
    private record Expired(String role, byte[] nonce) {}
}
