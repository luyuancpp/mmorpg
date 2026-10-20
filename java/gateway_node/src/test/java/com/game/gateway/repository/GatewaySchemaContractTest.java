package com.game.gateway.repository;

import com.game.gateway.entity.Announcement;
import com.game.gateway.entity.ZoneConfig;
import com.game.gateway.entity.ZoneWhitelist;
import org.hibernate.SessionFactory;
import org.hibernate.boot.MetadataSources;
import org.hibernate.boot.registry.StandardServiceRegistry;
import org.hibernate.boot.registry.StandardServiceRegistryBuilder;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.condition.EnabledIfEnvironmentVariable;
import org.springframework.core.io.ClassPathResource;
import org.springframework.jdbc.datasource.DriverManagerDataSource;
import org.springframework.jdbc.datasource.init.ScriptUtils;

import javax.sql.DataSource;
import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.Statement;
import java.util.Locale;
import java.util.UUID;

import static org.junit.jupiter.api.Assertions.*;

/**
 * 正式 schema.sql 必须通过三张 JPA 实体的 validate，而非由 create-drop 反向生成表后自证。
 * 默认 H2 只核契约；显式 TiDB 用例另建随机专用库，验证后删除，绝不接管已有目录库。
 */
class GatewaySchemaContractTest {

    private static final long MAX_UINT32 = 0xffff_ffffL;

    @Test
    void formalSchemaMatchesEntitiesAndKeepsUnsignedZoneRange() throws Exception {
        DriverManagerDataSource dataSource = new DriverManagerDataSource(
                "jdbc:h2:mem:gateway_schema_" + UUID.randomUUID().toString().replace("-", "")
                        + ";MODE=MySQL;DB_CLOSE_DELAY=-1", "sa", "");
        assertFormalSchema(dataSource);
    }

    @Test
    @EnabledIfEnvironmentVariable(named = "GATEWAY_SCHEMA_TIDB_URL", matches = ".+")
    void formalSchemaValidatesOnRealTiDbAndRoundTripsUnsignedZoneRange() throws Exception {
        String endpoint = System.getenv("GATEWAY_SCHEMA_TIDB_URL");
        assertEquals("jdbc:mysql://127.0.0.1:4000/", endpoint,
                "只允许本机 TiDB 无库名端点，禁止连接已有业务库");
        String user = System.getenv("GATEWAY_SCHEMA_TIDB_USER");
        String password = System.getenv("GATEWAY_SCHEMA_TIDB_PASSWORD");
        assertNotNull(user, "测试账号必须从环境注入");
        assertNotNull(password, "测试口令必须显式从环境注入，可为空但不猜测");
        String database = "gateway_schema_" + UUID.randomUUID().toString().replace("-", "") + "_test";
        boolean created = false;
        try (Connection server = DriverManager.getConnection(endpoint, user, password)) {
            try (Statement statement = server.createStatement(); ResultSet rows = statement.executeQuery("SELECT VERSION()")) {
                assertTrue(rows.next());
                assertTrue(rows.getString(1).toLowerCase(Locale.ROOT).contains("tidb"), "必须验证真实 TiDB");
            }
            try {
                try (Statement statement = server.createStatement()) {
                    statement.execute("CREATE DATABASE `" + database + "` CHARACTER SET utf8mb4");
                    created = true;
                }
                DriverManagerDataSource dataSource = new DriverManagerDataSource(endpoint + database
                        + "?useSSL=false&connectTimeout=3000&socketTimeout=10000", user, password);
                assertFormalSchema(dataSource);
            } finally {
                if (created) {
                    try (Statement statement = server.createStatement()) {
                        statement.execute("DROP DATABASE `" + database + "`");
                    }
                    try (PreparedStatement statement = server.prepareStatement(
                            "SELECT COUNT(*) FROM information_schema.SCHEMATA WHERE SCHEMA_NAME=?")) {
                        statement.setString(1, database);
                        try (ResultSet rows = statement.executeQuery()) {
                            assertTrue(rows.next());
                            assertEquals(0, rows.getInt(1), "随机专用测试库必须清理干净");
                        }
                    }
                }
            }
        }
    }

    private static void assertFormalSchema(DataSource dataSource) throws Exception {
        try (Connection connection = dataSource.getConnection()) {
            ScriptUtils.executeSqlScript(connection, new ClassPathResource("schema.sql"));
        }
        StandardServiceRegistry registry = new StandardServiceRegistryBuilder()
                .applySetting("hibernate.connection.datasource", dataSource)
                .applySetting("hibernate.dialect", "org.hibernate.dialect.MySQLDialect")
                .applySetting("hibernate.hbm2ddl.auto", "validate")
                .applySetting("hibernate.show_sql", false)
                .build();
        try {
            try (SessionFactory factory = new MetadataSources(registry)
                    .addAnnotatedClass(ZoneConfig.class)
                    .addAnnotatedClass(ZoneWhitelist.class)
                    .addAnnotatedClass(Announcement.class)
                    .buildMetadata().buildSessionFactory()) {
                ZoneConfig zone = new ZoneConfig();
                zone.setZoneId(MAX_UINT32);
                zone.setName("uint32-zone");
                ZoneWhitelist whitelist = new ZoneWhitelist();
                whitelist.setZoneId(MAX_UINT32);
                whitelist.setAccountId(7L);
                whitelist.setNote("uint32-whitelist");
                Announcement announcement = new Announcement();
                announcement.setTitle("schema-contract");
                announcement.setContent("正式建表后通过实体写入");
                factory.inTransaction(session -> {
                    session.persist(zone);
                    session.persist(whitelist);
                    session.persist(announcement);
                });
                factory.inSession(session -> {
                    assertEquals(MAX_UINT32, session.find(ZoneConfig.class, MAX_UINT32).getZoneId());
                    ZoneWhitelist restored = session.find(ZoneWhitelist.class, whitelist.getId());
                    assertNotNull(restored);
                    assertEquals(MAX_UINT32, restored.getZoneId());
                    assertEquals("uint32-whitelist", restored.getNote());
                    assertNotNull(session.find(Announcement.class, announcement.getId()));
                });
            }
        } finally {
            StandardServiceRegistryBuilder.destroy(registry);
        }
    }
}
