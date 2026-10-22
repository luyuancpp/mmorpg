package com.game.mall;

import static org.junit.jupiter.api.Assertions.*;
import com.zaxxer.hikari.HikariDataSource;
import java.net.URLEncoder;
import java.nio.charset.StandardCharsets;
import java.sql.Connection;
import java.sql.ResultSet;
import java.sql.Statement;
import java.util.ArrayList;
import java.util.List;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.ValueSource;
import org.springframework.boot.autoconfigure.jdbc.DataSourceProperties;
import org.springframework.jdbc.core.JdbcTemplate;
import org.springframework.jdbc.datasource.SingleConnectionDataSource;
import org.springframework.transaction.support.TransactionTemplate;

/** 仅显式 -Dtest=TiDbEnvironmentIT -Dcommerce.it.tidb=jdbc:mysql://127.0.0.1:24000 执行，不伪造数据库。 */
class TiDbEnvironmentIT {
    @Test void realTidbHasVerifiedSchemaAndEveryPhysicalConnectionUsesStrongSessions() throws Exception {
        assertNotNull(System.getProperty("commerce.it.tidb"), "此集成用例必须显式连接真实 TiDB");
        try (var f = new CommerceFixture()) {
            SchemaVerifier.verify(f.jdbc, f.source);
            DataSourceProperties properties = new DataSourceProperties();
            String snapshot = currentSnapshot(f);
            properties.setUrl(f.jdbcUrl + "?sessionVariables=" + URLEncoder.encode(
                    "tidb_read_consistency='weak',tidb_snapshot='" + snapshot + "'", StandardCharsets.UTF_8));
            properties.setUsername(((HikariDataSource) f.source).getUsername());
            properties.setPassword(((HikariDataSource) f.source).getPassword());
            try (var pool = (HikariDataSource) new DatabaseConfiguration().dataSource(properties, f.props)) {
                List<Connection> held = new ArrayList<>();
                try {
                    // 同时持有四个连接，不能靠同一条物理连接反复借还冒充初始化验证。
                    for (int i = 0; i < 4; i++) { held.add(pool.getConnection()); }
                    for (Connection connection : held) {
                        assertEquals(Connection.TRANSACTION_READ_COMMITTED, connection.getTransactionIsolation());
                        connection.setAutoCommit(false);
                        try (Statement statement = connection.createStatement()) {
                            statement.setQueryTimeout(5);
                            try (ResultSet ignored = statement.executeQuery("SELECT nonce FROM commerce_request_nonce WHERE caller_role='probe' AND nonce=X'00'")) { while (ignored.next()) { } }
                            try (ResultSet rows = statement.executeQuery("SELECT TIDB_VERSION(),@@session.tidb_txn_mode,@@session.tidb_replica_read,@@session.tidb_read_staleness,@@session.tidb_low_resolution_tso,@@session.sql_mode,(TIDB_CURRENT_TSO() >> 18),@@session.tidb_read_consistency,@@session.tidb_snapshot")) {
                                assertTrue(rows.next()); assertFalse(rows.getString(1).isBlank());
                                assertEquals("pessimistic", rows.getString(2)); assertEquals("leader", rows.getString(3));
                                assertEquals(0, rows.getLong(4)); assertEquals(0, rows.getInt(5)); assertTrue(rows.getString(6).contains("STRICT_TRANS_TABLES"));
                                assertEquals("strict", rows.getString(8)); assertEquals("", rows.getString(9));
                                assertTrue(rows.getLong(7) > 1_700_000_000_000L);
                            }
                        } finally { connection.rollback(); }
                    }
                } finally { for (Connection connection : held) { connection.close(); } }
            }
        }
    }
    @ParameterizedTest @ValueSource(strings = {"weak", "snapshot"})
    void startupRejectsWeakOrHistoricalSessions(String setting) throws Exception {
        assertNotNull(System.getProperty("commerce.it.tidb"));
        try (var f = new CommerceFixture()) {
            String snapshot = currentSnapshot(f);
            try (Connection connection = f.source.getConnection(); Statement statement = connection.createStatement()) {
                statement.setQueryTimeout(5);
                var single = new SingleConnectionDataSource(connection, true);
                try {
                    statement.execute("weak".equals(setting) ? "SET SESSION tidb_read_consistency='weak'"
                            : "SET SESSION tidb_snapshot='" + snapshot + "'");
                    assertThrows(IllegalStateException.class, () -> new DatabaseConfiguration()
                            .commerceSchemaReady(new JdbcTemplate(single), f.props, single).afterPropertiesSet());
                } finally {
                    statement.execute("SET SESSION tidb_snapshot='',tidb_read_consistency='strict'");
                }
            }
        }
    }
    private static String currentSnapshot(CommerceFixture f) {
        return new TransactionTemplate(f.manager).execute(status -> {
            f.jdbc.queryForList("SELECT nonce FROM commerce_request_nonce WHERE caller_role='probe' AND nonce=X'00'");
            return f.jdbc.queryForObject("SELECT CAST(TIDB_CURRENT_TSO() AS CHAR)", String.class);
        });
    }
    @Test void sqlTsoNonceIsSharedCaseSensitiveAndRejectsExpiredSignatures() {
        assertNotNull(System.getProperty("commerce.it.tidb"));
        try (var f = new CommerceFixture()) {
            NonceStore first = new NonceStore(f.jdbc, f.props, f.manager); NonceStore second = new NonceStore(f.jdbc, f.props, f.manager);
            long now = System.currentTimeMillis();
            first.claim("gateway", "NonceCase00000001", now);
            assertEquals(409, assertThrows(CommerceException.class, () -> second.claim("gateway", "NonceCase00000001", now)).status());
            second.claim("gateway", "noncecase00000001", now);
            assertEquals(401, assertThrows(CommerceException.class, () -> second.claim("gateway", "ExpiredNonce00001", now - 500_000)).status());
            assertEquals(401, assertThrows(CommerceException.class, () -> second.claim("gateway", "FutureNonce000001", now + 500_000)).status());
            assertEquals(2, f.count("commerce_request_nonce"));
        }
    }
}
