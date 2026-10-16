package com.game.mall;

import java.sql.*;
import java.util.*;
import javax.sql.DataSource;
import org.springframework.jdbc.core.JdbcTemplate;

final class SchemaVerifier {
    private static final Map<String, String> COLUMNS = Map.of(
        "commerce_schema_version", "version,storage_contract",
        "commerce_buyer_guard", "buyer_id",
        "pms_sku_stock", "id,product_id,sku_code,price_minor,currency,stock,lock_stock,published",
        "oms_order", "id,order_sn,buyer_id,request_id,sku_id,quantity,pay_amount,currency,status,create_time_ms,payment_deadline_ms,payment_time_ms",
        "oms_order_item", "order_sn,product_id,product_sku_id,product_sku_code,product_quantity,product_price_minor",
        "commerce_payment_receipt", "channel,transaction_id,order_sn,amount_minor,currency",
        "commerce_delivery_intent", "order_sn,buyer_id,sku_id,quantity,status,created_ms",
        "commerce_request_nonce", "caller_role,nonce,expires_ms");
    private SchemaVerifier() {}
    static void verify(JdbcTemplate jdbc, DataSource source) throws SQLException {
        List<Map<String, Object>> version = jdbc.queryForList("SELECT version,storage_contract FROM commerce_schema_version");
        if (version.size() != 1 || ((Number)version.get(0).get("version")).intValue() != 1 || !"mall-game-official-sku-v1-minor-units".equals(version.get(0).get("storage_contract"))) {
            throw new IllegalStateException("商城存储版本不兼容，必须使用独立迁移后的商城库");
        }
        try (Connection connection = source.getConnection()) {
            if (connection.getTransactionIsolation() != Connection.TRANSACTION_READ_COMMITTED) { throw new IllegalStateException("商城连接必须使用 READ-COMMITTED"); }
            for (var table : COLUMNS.entrySet()) {
                try (PreparedStatement statement = connection.prepareStatement("SELECT " + table.getValue() + " FROM " + table.getKey() + " WHERE 1=0"); ResultSet rows = statement.executeQuery()) {
                    ResultSetMetaData metadata = rows.getMetaData();
                    for (int i = 1; i <= metadata.getColumnCount(); i++) {
                        String column = metadata.getColumnName(i).toLowerCase(Locale.ROOT);
                        if (Set.of("request_id", "channel", "transaction_id", "nonce").contains(column) && metadata.getColumnType(i) != Types.VARBINARY) {
                            throw new IllegalStateException("商城不透明标识列必须使用 VARBINARY: " + table.getKey() + "." + column);
                        }
                        if (Set.of("price_minor", "pay_amount", "amount_minor", "product_price_minor").contains(column) && metadata.getColumnType(i) != Types.BIGINT) {
                            throw new IllegalStateException("商城金额列必须为最小单位 BIGINT: " + table.getKey() + "." + column);
                        }
                    }
                }
            }
            unique(connection, "commerce_schema_version", List.of("version"));
            unique(connection, "commerce_buyer_guard", List.of("buyer_id"));
            unique(connection, "pms_sku_stock", List.of("id"));
            unique(connection, "oms_order", List.of("order_sn")); unique(connection, "oms_order", List.of("buyer_id", "request_id"));
            unique(connection, "oms_order_item", List.of("order_sn"));
            unique(connection, "commerce_payment_receipt", List.of("channel", "transaction_id")); unique(connection, "commerce_payment_receipt", List.of("order_sn"));
            unique(connection, "commerce_delivery_intent", List.of("order_sn"));
            unique(connection, "commerce_request_nonce", List.of("caller_role", "nonce"));
            if (indexes(connection, "oms_order", false).values().stream().noneMatch(cols -> cols.size() >= 2 && cols.subList(0, 2).equals(List.of("buyer_id", "status")))) {
                throw new IllegalStateException("商城未支付订单计数缺买家状态索引");
            }
            if (indexes(connection, "commerce_request_nonce", false).values().stream().noneMatch(cols -> !cols.isEmpty() && cols.get(0).equals("expires_ms"))) {
                throw new IllegalStateException("商城 nonce 缺过期清理索引");
            }
        }
    }
    private static void unique(Connection connection, String table, List<String> expected) throws SQLException {
        if (!indexes(connection, table, true).containsValue(expected)) { throw new IllegalStateException("商城缺必要唯一约束: " + table + expected); }
    }
    private static Map<String, List<String>> indexes(Connection connection, String table, boolean unique) throws SQLException {
        DatabaseMetaData meta = connection.getMetaData();
        String actual = meta.storesUpperCaseIdentifiers() ? table.toUpperCase(Locale.ROOT) : table;
        Map<String, SortedMap<Integer, String>> collected = new HashMap<>();
        try (ResultSet rows = meta.getIndexInfo(connection.getCatalog(), connection.getSchema(), actual, unique, false)) {
            while (rows.next()) {
                String name = rows.getString("INDEX_NAME"); String column = rows.getString("COLUMN_NAME");
                if (name != null && column != null) { collected.computeIfAbsent(name, ignored -> new TreeMap<>()).put(rows.getInt("ORDINAL_POSITION"), column.toLowerCase(Locale.ROOT)); }
            }
        }
        Map<String, List<String>> result = new HashMap<>(); collected.forEach((name, values) -> result.put(name, List.copyOf(values.values()))); return result;
    }
}
