package com.game.mall;

import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.SQLException;
import java.util.Properties;
import java.util.UUID;
import org.junit.jupiter.api.AfterAll;
import org.springframework.test.annotation.DirtiesContext;
import org.springframework.test.context.DynamicPropertyRegistry;
import org.springframework.test.context.DynamicPropertySource;

/**
 * 显式执行的本地真 TiDB HTTP 契约：继承同一套 HTTP 行为断言，并启用生产数据库门禁。
 * -Dtest=CommerceTiDBHttpIT -Dcommerce.it.tidb=jdbc:mysql://127.0.0.1:4000/
 * 不接受已有库名或远端地址；没有参数直接失败，不把未运行集成伪装成通过。
 */
@DirtiesContext(classMode = DirtiesContext.ClassMode.AFTER_CLASS)
class CommerceTiDBHttpIT extends CommerceHttpFlowTest {
    private static String baseUrl;
    private static String database;
    private static Properties credentials;

    @DynamicPropertySource
    static synchronized void realDatabase(DynamicPropertyRegistry registry) throws SQLException {
        if (database == null) {
            String configured = System.getProperty("commerce.it.tidb", "");
            if (!configured.matches("jdbc:mysql://(127\\.0\\.0\\.1|localhost):4000/?")) {
                throw new IllegalStateException("真 TiDB HTTP 测试必须显式指定本机 4000 端口，不得传入已有库名");
            }
            baseUrl = configured.endsWith("/") ? configured : configured + "/";
            credentials = new Properties();
            credentials.setProperty("user", System.getenv().getOrDefault("COMMERCE_IT_TIDB_USER", "root"));
            credentials.setProperty("password", System.getenv().getOrDefault("COMMERCE_IT_TIDB_PASSWORD", ""));
            credentials.setProperty("connectTimeout", "3000");
            credentials.setProperty("socketTimeout", "6000");
            String candidate = "mallhttpit_" + UUID.randomUUID().toString().replace("-", "").substring(0, 16);
            requireOwnedName(candidate);
            try (Connection admin = DriverManager.getConnection(baseUrl, credentials);
                 var statement = admin.createStatement()) {
                statement.setQueryTimeout(6);
                statement.execute("CREATE DATABASE `" + candidate + "`");
            }
            database = candidate;
        }
        registry.add("spring.datasource.url", () -> baseUrl + database);
        registry.add("spring.datasource.username", () -> credentials.getProperty("user"));
        registry.add("spring.datasource.password", () -> credentials.getProperty("password"));
        registry.add("commerce.run-mode", () -> "prod");
        registry.add("commerce.require-tidb", () -> "true");
    }

    @AfterAll
    static synchronized void removeOnlyThisTestDatabase() throws SQLException {
        if (database == null) { return; }
        requireOwnedName(database);
        try (Connection admin = DriverManager.getConnection(baseUrl, credentials);
             var statement = admin.createStatement()) {
            statement.setQueryTimeout(6);
            statement.execute("DROP DATABASE `" + database + "`");
        }
        database = null;
    }

    private static void requireOwnedName(String value) {
        if (!value.matches("mallhttpit_[0-9a-f]{16}")) {
            throw new IllegalStateException("拒绝操作不属于本测试的新库");
        }
    }
}
