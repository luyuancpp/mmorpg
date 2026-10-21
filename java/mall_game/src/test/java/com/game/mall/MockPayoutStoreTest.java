package com.game.mall;

import static com.game.mall.RmbConsignmentStoreTest.*;
import static org.junit.jupiter.api.Assertions.*;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.*;

import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicLong;
import java.util.function.LongSupplier;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.support.StaticListableBeanFactory;

/** 与RMB测试共用随机SQL夹具；仅在结算边界注入故障，不伪造渠道成功或玩家到账。 */
class MockPayoutStoreTest {
    @Test void 提供方先独立commit_结算失败保持未知_重启沿原流水恢复() {
        try (var f = new Fixture()) {
            var withdrawal = f.earned(1);
            var unavailable = mock(RmbConsignmentStore.class);
            when(unavailable.completeWithdrawal(any())).thenThrow(new CommerceException(503, "本例结算不可用"));
            var request = new MockPayoutStore.Pay(withdrawal.withdrawalId(), "first-window-request");
            rejects(503, () -> worker(f, unavailable, () -> 0).pay(request));
            var original = fact(f, withdrawal.withdrawalId());
            assertEquals(0, applied(f, withdrawal.withdrawalId()));
            assertEquals(0, f.db.count("commerce_withdrawal_receipt"));
            assertEquals("REQUESTED", f.rmb.getWithdrawal(new RmbConsignmentApi.WithdrawalQuery(SELLER, withdrawal.withdrawalId())).state());
            assertEquals(95, f.db.value("commerce_seller_income", "reserved_fen"));
            assertEquals(new MockPaymentApi.Reconcile(1, 1, 0, 0, false, false), f.newPayout(() -> 0).reconcile());
            assertEquals(original, fact(f, withdrawal.withdrawalId()));
            assertEquals(1, applied(f, withdrawal.withdrawalId()));
            assertEquals("PAID", f.rmb.getWithdrawal(new RmbConsignmentApi.WithdrawalQuery(SELLER, withdrawal.withdrawalId())).state());
            assertEquals(1, f.db.count("commerce_withdrawal_receipt"));
            assertEquals(0, f.newPayout(() -> 0).reconcile().attempted());
            rejects(409, () -> f.payout.pay(new MockPayoutStore.Pay(withdrawal.withdrawalId(), "replacement-request")));
            var other = f.earned(2);
            rejects(409, () -> f.payout.pay(new MockPayoutStore.Pay(other.withdrawalId(), request.requestId())));
            assertEquals(1, f.db.count("commerce_mock_payout"));
            assertEquals("REQUESTED", f.rmb.getWithdrawal(new RmbConsignmentApi.WithdrawalQuery(SELLER, other.withdrawalId())).state());
        }
    }

    @Test void 结算已commit但applied预算耗尽_原流水再次投递不双扣() {
        try (var f = new Fixture()) {
            var withdrawal = f.earned(1);
            var nanos = new AtomicLong();
            var boundary = mock(RmbConsignmentStore.class);
            when(boundary.completeWithdrawal(any())).thenAnswer(call -> {
                var settled = f.rmb.completeWithdrawal(call.getArgument(0));
                nanos.set(TimeUnit.SECONDS.toNanos(15));
                return settled;
            });
            var input = new MockPayoutStore.Pay(withdrawal.withdrawalId(), "second-window-request");
            rejects(503, () -> worker(f, boundary, nanos::get).pay(input));
            var original = fact(f, withdrawal.withdrawalId());
            assertEquals(0, applied(f, withdrawal.withdrawalId()));
            assertEquals(1, f.db.count("commerce_withdrawal_receipt"));
            assertEquals(95, f.db.value("commerce_seller_income", "paid_fen"));
            assertEquals(0, f.db.value("commerce_seller_income", "reserved_fen"));
            assertEquals(1, f.newPayout(() -> 0).reconcile().succeeded());
            assertEquals(original, fact(f, withdrawal.withdrawalId()));
            assertEquals(1, applied(f, withdrawal.withdrawalId()));
            assertEquals(1, f.db.count("commerce_withdrawal_receipt"));
            assertEquals(95, f.db.value("commerce_seller_income", "paid_fen"));
            assertEquals("PAID", f.payout.pay(input).state());
        }
    }

    @Test void 前二十坏原件不饿死尾五_游标绕回与新worker不造第二流水() {
        try (var f = new Fixture()) {
            var unavailable = mock(RmbConsignmentStore.class);
            when(unavailable.completeWithdrawal(any())).thenThrow(new CommerceException(503, "本例留下耐久待办"));
            var pending = worker(f, unavailable, () -> 0);
            for (int i = 1; i <= 25; i++) {
                int index = i;
                var withdrawal = f.earned(i);
                rejects(503, () -> pending.pay(new MockPayoutStore.Pay(withdrawal.withdrawalId(), "fair-payout-request-" + index)));
            }
            var ids = f.db.jdbc.queryForList("SELECT withdrawal_id FROM commerce_mock_payout ORDER BY withdrawal_id", String.class);
            var tailOriginal = ids.subList(20, 25).stream().map(id -> fact(f, id)).toList();
            for (int i = 0; i < 20; i++) {
                f.db.jdbc.update("UPDATE commerce_mock_payout SET transaction_id=? WHERE withdrawal_id=?", bytes("broken!" + i), ids.get(i));
            }
            var result = f.payout.reconcile();
            assertEquals(new MockPaymentApi.Reconcile(25, 5, 20, 0, false, false), result);
            assertEquals(ids.subList(20, 25), f.db.jdbc.queryForList("SELECT withdrawal_id FROM commerce_withdrawal_receipt ORDER BY withdrawal_id", String.class));
            assertEquals(20, f.db.jdbc.queryForObject("SELECT COUNT(*) FROM commerce_mock_payout WHERE applied=0", Integer.class));
            assertEquals(20 * 95, f.db.value("commerce_seller_income", "reserved_fen"));
            assertEquals(5 * 95, f.db.value("commerce_seller_income", "paid_fen"));
            assertEquals(20, f.payout.reconcile().failed());
            assertEquals(20, f.newPayout(() -> 0).reconcile().failed());
            assertEquals(tailOriginal, ids.subList(20, 25).stream().map(id -> fact(f, id)).toList());
            assertEquals(25, f.db.count("commerce_mock_payout"));
            assertEquals(5, f.db.count("commerce_withdrawal_receipt"));
        }
    }

    @Test void 读取后预算耗尽不开始项_恢复原游标_扫描SQL错误不冒成功() {
        try (var f = new Fixture()) {
            var withdrawal = f.earned(1);
            var unavailable = mock(RmbConsignmentStore.class);
            when(unavailable.completeWithdrawal(any())).thenThrow(new CommerceException(503, "本例保留待办"));
            rejects(503, () -> worker(f, unavailable, () -> 0).pay(new MockPayoutStore.Pay(withdrawal.withdrawalId(), "budget-pending-request")));
            // 第7次单调时间读取是本轮max/next两次SQL完成后的开始门禁；不依赖机器响应速度。
            var calls = new AtomicInteger();
            var scanning = f.newPayout(() -> calls.incrementAndGet() >= 7 ? TimeUnit.SECONDS.toNanos(3) : 0);
            var first = scanning.reconcile();
            assertEquals(0, first.attempted()); assertEquals(0, first.scanFailures()); assertTrue(first.budgetExhausted());
            assertEquals(0, f.db.count("commerce_withdrawal_receipt"));
            assertEquals(1, scanning.reconcile().succeeded());
            f.db.jdbc.execute("DROP TABLE commerce_mock_payout");
            var failure = f.newPayout(() -> 0).reconcile();
            assertEquals(0, failure.attempted()); assertEquals(1, failure.scanFailures()); assertTrue(failure.unavailable());
        }
    }

    @Test void 两独立worker并发只提交同一出款_所有future错误回主线程() throws Exception {
        try (var f = new Fixture()) {
            var withdrawal = f.earned(1);
            var input = new MockPayoutStore.Pay(withdrawal.withdrawalId(), "parallel-payout-request");
            var second = f.newPayout(() -> 0);
            var results = parallel(() -> f.payout.pay(input), () -> second.pay(input));
            assertEquals(results.getFirst(), results.getLast()); assertEquals("PAID", results.getFirst().state());
            assertEquals(1, f.db.count("commerce_mock_payout")); assertEquals(1, f.db.count("commerce_withdrawal_receipt"));
            assertEquals(95, f.db.value("commerce_seller_income", "paid_fen"));
            assertEquals(0, f.db.value("commerce_seller_income", "reserved_fen"));
        }
    }

    @Test void 默认拒本地出款_坏回执与schema缺列拒绝_正式V5可迁移() throws Exception {
        try (var f = new Fixture()) {
            var withdrawal = f.earned(1);
            f.db.props.setMockPaymentEnabled(false);
            rejects(503, () -> f.payout.pay(new MockPayoutStore.Pay(withdrawal.withdrawalId(), "disabled-payout-request")));
            rejects(503, f.payout::reconcile);
            assertEquals(0, f.db.count("commerce_mock_payout"));
            f.db.props.setMockPaymentEnabled(true);
            rejects(400, () -> f.payout.pay(new MockPayoutStore.Pay("AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA", "bad-id-request")));
            var forged = new RmbConsignmentApi.WithdrawalReceipt(withdrawal.withdrawalId(), "mock", "mock-payout-00000000-0000-0000-0000-000000000000", 95, "CNY", true);
            rejects(409, () -> f.rmb.completeWithdrawal(forged));
            assertEquals(0, f.db.count("commerce_withdrawal_receipt"));
            f.db.jdbc.execute("ALTER TABLE commerce_withdrawal DROP COLUMN transaction_id");
            assertThrows(Exception.class, () -> SchemaVerifier.verify(f.db.jdbc, f.db.source));
        }
        try (var f = new Fixture()) {
            f.db.sku(1, 10, 125);
            var original = f.db.store.create("1", new CommerceApi.CreateOrder("v5-preserved-order", 1, 2, "测试账号", "1"));
            for (String table : List.of("commerce_mock_payout", "commerce_consignment_return", "commerce_withdrawal_receipt", "commerce_withdrawal_allocation", "commerce_withdrawal", "commerce_seller_income", "commerce_seller_guard", "commerce_consignment_order", "commerce_consignment", "commerce_official_owner")) {
                f.db.jdbc.execute("DROP TABLE " + table);
            }
            assertThrows(Exception.class, () -> SchemaVerifier.verify(f.db.jdbc, f.db.source));
            f.db.migrate("db/migration/V5__rmb_consignment.sql");
            SchemaVerifier.verify(f.db.jdbc, f.db.source);
            // 迁移不能凭旧player_id猜账号；夹具明确持有原件后才可恢复原身份。
            assertEquals(503, assertThrows(CommerceException.class, () -> f.db.store.get("1",
                new CommerceApi.OwnedOrder(original.orderId(), "测试账号", "1"))).status());
            f.db.jdbc.update("INSERT INTO commerce_official_owner(order_sn,buyer_account,buyer_owner_revision) VALUES(?,?,?)",
                original.orderId(), bytes("测试账号"), bytes("1"));
            assertEquals(original, f.db.store.get("1", new CommerceApi.OwnedOrder(original.orderId(), "测试账号", "1")));
            assertEquals(10, f.db.value("pms_sku_stock", "stock"));
            assertEquals(2, f.db.value("pms_sku_stock", "lock_stock"));
        }
    }

    private static MockPayoutStore worker(Fixture f, RmbConsignmentStore settlements, LongSupplier nanos) {
        var beans = new StaticListableBeanFactory(); beans.addBean("settlements", settlements);
        return new MockPayoutStore(f.db.jdbc, f.db.manager, f.db.props, beans.getBeanProvider(RmbConsignmentStore.class), f.clock, nanos);
    }
    private static int applied(Fixture f, String id) { return f.db.jdbc.queryForObject("SELECT applied FROM commerce_mock_payout WHERE withdrawal_id=?", Integer.class, id); }
    private static List<Object> fact(Fixture f, String id) {
        return f.db.jdbc.queryForObject("SELECT withdrawal_id,request_id,transaction_id,amount_fen,successful,created_ms FROM commerce_mock_payout WHERE withdrawal_id=?",
            (row, index) -> List.of(row.getString(1), java.util.HexFormat.of().formatHex(row.getBytes(2)), java.util.HexFormat.of().formatHex(row.getBytes(3)), row.getLong(4), row.getInt(5), row.getLong(6)), id);
    }
}
