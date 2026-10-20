package com.game.mall;

import static org.junit.jupiter.api.Assertions.*;
import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicInteger;
import org.junit.jupiter.api.Test;

class CommerceStoreTest {
    private CommerceApi.CreateOrder request(String key, long sku, int quantity) { return new CommerceApi.CreateOrder(key, sku, quantity); }
    private CommerceApi.Payment payment(CommerceApi.Order order, String transaction) { return new CommerceApi.Payment(order.orderId(), "test", transaction, order.amountMinor(), order.currency()); }

    @Test void requestRetryKeepsOriginalPriceAndRejectsParameterChanges() {
        try (var f = new CommerceFixture()) {
            f.sku(1, 5, 125);
            CommerceApi.Order first = f.store.create("18446744073709551615", request("request-001", 1, 2));
            f.jdbc.update("UPDATE pms_sku_stock SET price_minor=999 WHERE id=1");
            assertEquals(first, f.store.create("18446744073709551615", request("request-001", 1, 2)));
            assertEquals(250, first.amountMinor()); assertEquals(2, f.value("pms_sku_stock", "lock_stock"));
            assertEquals(409, assertThrows(CommerceException.class, () -> f.store.create("18446744073709551615", request("request-001", 1, 1))).status());
            assertEquals(404, assertThrows(CommerceException.class, () -> f.store.get("2", first.orderId())).status());
            assertEquals(1_760_000_900_000L, first.paymentDeadlineMs());
        }
    }
    @Test void concurrentRequestsCannotOversell() throws Exception {
        try (var f = new CommerceFixture()) {
            f.sku(1, 2, 100); AtomicInteger success = new AtomicInteger();
            parallel(16, index -> {
                try { f.store.create(Integer.toString(100 + index), request("concurrent-" + index, 1, 1)); success.incrementAndGet(); }
                catch (CommerceException refused) { assertEquals(409, refused.status()); }
            });
            assertEquals(2, success.get()); assertEquals(2, f.count("oms_order")); assertEquals(2, f.value("pms_sku_stock", "lock_stock"));
        }
    }
    @Test void sameRequestAcrossReplicasCreatesOnlyOneOrder() throws Exception {
        try (var f = new CommerceFixture()) {
            f.sku(1, 100, 100); Set<String> ids = ConcurrentHashMap.newKeySet();
            parallel(16, index -> ids.add(f.store.create("1", request("same-key-001", 1, 1)).orderId()));
            assertEquals(1, ids.size()); assertEquals(1, f.count("oms_order_item")); assertEquals(1, f.value("pms_sku_stock", "lock_stock"));
        }
    }
    @Test void sameRequestCannotSwitchSkuUnderConcurrency() throws Exception {
        try (var f = new CommerceFixture()) {
            f.sku(1, 10, 100); f.sku(2, 10, 200); AtomicInteger success = new AtomicInteger();
            parallel(2, index -> {
                try { f.store.create("1", request("different-sku-001", index + 1, 1)); success.incrementAndGet(); }
                catch (CommerceException refused) { assertEquals(409, refused.status()); }
            });
            assertEquals(1, success.get()); assertEquals(1, f.count("oms_order")); assertEquals(1, f.value("pms_sku_stock", "lock_stock"));
        }
    }
    @Test void buyerLimitIsRecheckedUnderBuyerGuard() throws Exception {
        try (var f = new CommerceFixture()) {
            f.sku(1, 100, 100); AtomicInteger success = new AtomicInteger();
            parallel(16, index -> {
                try { f.store.create("1", request("buyer-limit-" + index, 1, 1)); success.incrementAndGet(); }
                catch (CommerceException refused) { assertEquals(429, refused.status()); }
            });
            assertEquals(5, success.get()); assertEquals(5, f.count("oms_order")); assertEquals(5, f.value("pms_sku_stock", "lock_stock"));
        }
    }
    @Test void paymentAndCancelSerializeToOneTerminalDecision() throws Exception {
        try (var f = new CommerceFixture()) {
            f.sku(1, 1, 100); var order = f.store.create("1", request("pay-cancel-001", 1, 1));
            parallel(2, index -> {
                try { if (index == 0) { f.store.cancel("1", order.orderId()); } else { f.store.recordTrustedPayment(payment(order, "pay-race")); } }
                catch (CommerceException refused) { assertEquals(409, refused.status()); }
            });
            String state = f.store.get("1", order.orderId()).state();
            assertEquals(0, f.value("pms_sku_stock", "lock_stock"));
            if (state.equals("REFUND_PENDING")) { assertEquals(1, f.value("pms_sku_stock", "stock")); assertEquals(1, f.count("commerce_payment_receipt")); assertEquals(0, f.count("commerce_delivery_intent"));assertEquals(1,f.count("commerce_refund_intent")); }
            else { assertEquals("DELIVERY_PENDING", state); assertEquals(0, f.value("pms_sku_stock", "stock")); assertEquals(1, f.count("commerce_payment_receipt")); assertEquals(1, f.count("commerce_delivery_intent")); }
        }
    }
    @Test void duplicatePaymentNeverDeductsTwiceAndCannotCompleteWithoutDelivery() {
        try (var f = new CommerceFixture()) {
            f.sku(1, 3, 100); var order = f.store.create("1", request("payment-001", 1, 2));
            var paid = f.store.recordTrustedPayment(payment(order, "paid-001"));
            assertEquals(paid, f.store.recordTrustedPayment(payment(order, "paid-001")));
            assertEquals("DELIVERY_PENDING", paid.state()); assertEquals(1, f.value("pms_sku_stock", "stock"));
            assertEquals(1, f.count("commerce_delivery_intent")); assertEquals(1, f.count("commerce_payment_receipt"));
            assertEquals(409, assertThrows(CommerceException.class, () -> f.store.cancel("1", order.orderId())).status());
            assertEquals(409, assertThrows(CommerceException.class, () -> f.store.recordTrustedPayment(payment(order, "paid-002"))).status());
        }
    }
    @Test void cancelIsIdempotentAndLatePaymentRequiresRefundHandling() {
        try (var f = new CommerceFixture()) {
            f.sku(1, 2, 100); var order = f.store.create("1", request("cancel-001", 1, 1));
            assertEquals(f.store.cancel("1", order.orderId()), f.store.cancel("1", order.orderId()));
            assertEquals(0, f.value("pms_sku_stock", "lock_stock")); assertEquals(2, f.value("pms_sku_stock", "stock"));
            assertEquals("REFUND_PENDING", f.store.recordTrustedPayment(payment(order, "late-payment")).state());
            assertEquals(1, f.count("commerce_payment_receipt"));
            assertEquals(1,f.count("commerce_refund_intent"));
        }
    }
    @Test void wrongAmountAndBrokenReservationRollbackAllPaymentFacts() {
        try (var f = new CommerceFixture()) {
            f.sku(1, 2, 100); var order = f.store.create("1", request("rollback-001", 1, 1));
            assertThrows(CommerceException.class, () -> f.store.recordTrustedPayment(new CommerceApi.Payment(order.orderId(), "test", "bad-amount", 99, "CNY")));
            assertThrows(CommerceException.class, () -> f.store.recordTrustedPayment(new CommerceApi.Payment(order.orderId(), "test", "bad-currency", 100, "USD")));
            f.jdbc.update("UPDATE pms_sku_stock SET lock_stock=0 WHERE id=1");
            assertThrows(CommerceException.class, () -> f.store.recordTrustedPayment(payment(order, "broken-reservation")));
            assertEquals(0, f.count("commerce_payment_receipt")); assertEquals(0, f.count("commerce_delivery_intent"));
            assertEquals("AWAITING_PAYMENT", f.store.get("1", order.orderId()).state()); assertEquals(2, f.value("pms_sku_stock", "stock"));
        }
    }
    @Test void concurrentOrdersCannotShareOnePaymentTransaction() throws Exception {
        try (var f = new CommerceFixture()) {
            f.sku(1, 2, 100); var first = f.store.create("1", request("first-order-001", 1, 1)); var second = f.store.create("2", request("second-order-001", 1, 1));
            AtomicInteger success = new AtomicInteger();
            parallel(2, index -> {
                try { f.store.recordTrustedPayment(payment(index == 0 ? first : second, "one-transaction")); success.incrementAndGet(); }
                catch (CommerceException refused) { assertEquals(409, refused.status()); }
            });
            assertEquals(1, success.get()); assertEquals(1, f.count("commerce_payment_receipt")); assertEquals(1, f.count("commerce_delivery_intent"));
            assertEquals(1, f.value("pms_sku_stock", "stock")); assertEquals(1, f.value("pms_sku_stock", "lock_stock"));
        }
    }
    @Test void requestAndPaymentOpaqueIdentifiersAreCaseSensitive() {
        try (var f = new CommerceFixture()) {
            f.sku(1, 10, 100);
            var first = f.store.create("1", request("OpaqueKeyAAA", 1, 1));
            var second = f.store.create("1", request("opaquekeyaaa", 1, 1));
            assertNotEquals(first.orderId(), second.orderId());
            f.store.recordTrustedPayment(payment(first, "PaymentCase")); f.store.recordTrustedPayment(payment(second, "paymentcase"));
            assertEquals(2, f.count("commerce_payment_receipt")); assertEquals(8, f.value("pms_sku_stock", "stock"));
            var third = f.store.create("1", request("third-key-001", 1, 1));
            assertThrows(CommerceException.class, () -> f.store.recordTrustedPayment(payment(third, "PaymentCase")));
            assertEquals("AWAITING_PAYMENT", f.store.get("1", third.orderId()).state());
        }
    }
    @Test void unpublishedAndOverflowingProductsNeverReserve() {
        try (var f = new CommerceFixture()) {
            f.sku(1, 10000, Long.MAX_VALUE); assertThrows(CommerceException.class, () -> f.store.create("1", request("overflow-001", 1, 10000)));
            f.jdbc.update("UPDATE pms_sku_stock SET price_minor=100,published=0 WHERE id=1"); assertThrows(CommerceException.class, () -> f.store.create("1", request("unpublished-001", 1, 1)));
            assertEquals(0, f.count("oms_order")); assertEquals(0, f.value("pms_sku_stock", "lock_stock"));
        }
    }
    private static void parallel(int count, ThrowingConsumer action) throws Exception {
        ExecutorService executor = Executors.newFixedThreadPool(8); CountDownLatch start = new CountDownLatch(1);
        try {
            List<Future<?>> results = new ArrayList<>();
            for (int i = 0; i < count; i++) { int index = i; results.add(executor.submit(() -> { start.await(); action.run(index); return null; })); }
            start.countDown(); for (Future<?> result : results) { result.get(20, TimeUnit.SECONDS); }
        } finally { executor.shutdownNow(); assertTrue(executor.awaitTermination(5, TimeUnit.SECONDS)); }
    }
    @FunctionalInterface private interface ThrowingConsumer { void run(int index) throws Exception; }
}
