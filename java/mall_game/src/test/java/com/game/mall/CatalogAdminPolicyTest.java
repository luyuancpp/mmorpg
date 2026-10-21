package com.game.mall;

import static org.junit.jupiter.api.Assertions.*;

import java.util.List;
import org.junit.jupiter.api.Test;

class CatalogAdminPolicyTest {
    @Test void 管理密钥可不配置_配置后必须独立且足够强() {
        var props = new CommerceProperties();
        props.setGatewaySecret("catalog-policy-test-gateway-key-00001");
        props.setPaymentSecret("catalog-policy-test-payment-key-00002");
        props.setDeliverySecret("catalog-policy-test-delivery-key-0003");
        assertDoesNotThrow(props::validate);
        for (String invalid : List.of("short", props.getGatewaySecret(), props.getPaymentSecret(), props.getDeliverySecret())) {
            props.setAdminSecret(invalid);
            assertThrows(IllegalStateException.class, props::validate);
        }
        props.setAdminSecret("catalog-policy-test-admin-key-0000004");
        assertDoesNotThrow(props::validate);
        props.setAdminSubject("33");
        assertThrows(IllegalStateException.class, props::validate);
    }

    @Test void V2升级必须补管理结构_保留既有订单和库存预占() throws Exception {
        try (var f = new CommerceFixture()) {
            f.sku(1, 5, 125);
            var original = f.store.create("1", new CommerceApi.CreateOrder("migration-order-001", 1, 2, "测试账号", "1"));
            var grant = f.jdbc.queryForMap("SELECT * FROM commerce_order_grant WHERE order_sn=?", original.orderId());
            // 仅操作本例随机库：还原V2形状，实际执行发给部署的同一份升级SQL。
            f.jdbc.execute("DROP TABLE commerce_admin_command");
            f.jdbc.execute("ALTER TABLE pms_sku_stock DROP COLUMN catalog_revision");
            assertThrows(Exception.class, () -> SchemaVerifier.verify(f.jdbc, f.source));
            f.migrate("db/migration/V3__catalog_admin.sql");
            SchemaVerifier.verify(f.jdbc, f.source);
            assertEquals(original, f.store.get("1", new CommerceApi.OwnedOrder(original.orderId(), "测试账号", "1")));
            assertEquals(grant, f.jdbc.queryForMap("SELECT * FROM commerce_order_grant WHERE order_sn=?", original.orderId()));
            assertEquals(5, f.value("pms_sku_stock", "stock"));
            assertEquals(2, f.value("pms_sku_stock", "lock_stock"));
            assertEquals(0, f.value("pms_sku_stock", "catalog_revision"));
            assertEquals(0, f.count("commerce_admin_command"));
        }
    }
}
