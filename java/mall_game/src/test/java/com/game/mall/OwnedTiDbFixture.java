package com.game.mall;

import java.net.InetAddress;
import java.net.URI;
import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.SQLException;
import java.util.Properties;

/** 仅测试的端点与建库前探针；合法端点仍须由执行者核实属于本任务。 */
final class OwnedTiDbFixture {
    private OwnedTiDbFixture() {}

    static String endpoint(String configured) {
        try {
            if (configured == null || !configured.startsWith("jdbc:mysql://")) { throw new IllegalArgumentException(); }
            URI uri = URI.create(configured.substring("jdbc:".length()));
            String host = uri.getHost();
            if (host == null) { throw new IllegalArgumentException(); }
            if (host.startsWith("[") && host.endsWith("]")) { host = host.substring(1, host.length() - 1); }
            // 先限制为数字IP，禁止InetAddress因localhost或其它名称而查询DNS。
            if (!(host.matches("[0-9]+(?:\\.[0-9]+){3}") || (host.contains(":") && host.matches("[0-9A-Fa-f:]+")))
                || !InetAddress.getByName(host).isLoopbackAddress() || uri.getPort() < 1 || uri.getPort() > 65535
                || uri.getRawUserInfo() != null || uri.getRawQuery() != null || uri.getRawFragment() != null
                || !(uri.getRawPath().isEmpty() || uri.getRawPath().equals("/"))) {
                throw new IllegalArgumentException();
            }
            return configured.endsWith("/") ? configured : configured + "/";
        } catch (IllegalArgumentException | java.net.UnknownHostException invalid) {
            throw new IllegalArgumentException("真实TiDB夹具仅接受显式数值loopback合法非零端口，不得指定已有库、凭据或查询参数");
        }
    }

    static Connection admin(String configured, Properties credentials) throws SQLException {
        // 先在系统库验证TiDB能力和当前强读会话，后续DDL只操作调用方生成并核验的随机自有库。
        Connection connection = DriverManager.getConnection(endpoint(configured) + "mysql", credentials);
        boolean accepted = false;
        try {
            connection.setTransactionIsolation(Connection.TRANSACTION_READ_COMMITTED);
            try (var statement = connection.createStatement()) {
                statement.setQueryTimeout(5);
                statement.execute("SET SESSION tidb_snapshot='',tidb_txn_mode='pessimistic',tidb_replica_read='leader',tidb_read_consistency='strict',tidb_read_staleness=0,tidb_low_resolution_tso=OFF");
                try (var rows = statement.executeQuery("SELECT TIDB_VERSION(),@@tidb_txn_mode,@@tidb_replica_read,@@tidb_read_consistency,@@tidb_read_staleness,@@tidb_low_resolution_tso,@@tidb_snapshot,@@transaction_isolation")) {
                    if (!rows.next() || rows.getString(1) == null || rows.getString(1).isBlank()
                        || !"pessimistic".equals(rows.getString(2)) || !"leader".equals(rows.getString(3))
                        || !"strict".equals(rows.getString(4)) || rows.getLong(5) != 0 || rows.getInt(6) != 0
                        || !"".equals(rows.getString(7)) || !"READ-COMMITTED".equals(rows.getString(8))) {
                        throw new SQLException("真实TiDB夹具建库前强读会话探针不符");
                    }
                }
            }
            accepted = true;
            return connection;
        } finally {
            if (!accepted) { connection.close(); }
        }
    }
}
