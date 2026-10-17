package com.game.mall;

import java.util.*;
import org.springframework.jdbc.core.JdbcTemplate;
import org.springframework.stereotype.Component;
import org.springframework.transaction.PlatformTransactionManager;
import org.springframework.transaction.TransactionDefinition;
import org.springframework.transaction.support.TransactionTemplate;

/** Mall侧可靠交付意图；相同order只能绑定一个trade合同，未知结果保留原意图。 */
@Component
public final class DeliveryStore {
    private final JdbcTemplate jdbc;
    private final TransactionTemplate tx;
    public DeliveryStore(JdbcTemplate jdbc, PlatformTransactionManager manager) {
        this.jdbc = jdbc;
        tx = new TransactionTemplate(manager);
        tx.setIsolationLevel(TransactionDefinition.ISOLATION_READ_COMMITTED);
        tx.setTimeout(5);
    }
    public DeliveryApi.Skus skus() {
        return skus(new DeliveryApi.SkuPage(0, 100));
    }
    public DeliveryApi.Skus skus(DeliveryApi.SkuPage page) {
        if (page == null || page.afterSkuId() < 0 || page.limit() < 1 || page.limit() > 100)
            throw new CommerceException(400, "商品分页非法");
        return new DeliveryApi.Skus(jdbc.query("SELECT s.id,g.title,s.price_minor,s.currency,s.stock-s.lock_stock FROM "
                + "pms_sku_stock s JOIN commerce_sku_grant g ON g.sku_id=s.id WHERE "
                + "s.published=1 AND s.stock>s.lock_stock AND s.id>? ORDER BY s.id LIMIT ?",
            (r, n)
                -> new DeliveryApi.Sku(r.getLong(1), r.getString(2), r.getLong(3), r.getString(4), r.getLong(5)),
            page.afterSkuId(), page.limit()));
    }
    public List<DeliveryApi.Intent> poll(int limit) {
        if (limit < 1 || limit > 100)
            throw new CommerceException(400, "交付批量非法");
        return jdbc.query("SELECT "
                          + "d.order_sn,d.buyer_id,d.sku_id,d.quantity,g.item_config_id,g.count_per_unit,g.title,o.pay_"
                          + "amount,o.currency "
                + "FROM commerce_delivery_intent d JOIN commerce_order_grant g ON g.order_sn=d.order_sn JOIN oms_order "
                  + "o ON o.order_sn=d.order_sn "
                + "WHERE d.status='PENDING' ORDER BY d.created_ms,d.order_sn LIMIT ?",
            (r, n)
                -> {
                long count = Math.multiplyExact(r.getInt(4), (long) r.getInt(6));
                if (count < 1 || count > 4096)
                    throw new CommerceException(503, "已冻结的SKU游戏数量越界");
                return new DeliveryApi.Intent(r.getString(1), r.getString(2), r.getLong(3), r.getInt(4), r.getInt(5),
                    (int) count, r.getString(7), r.getLong(8), r.getString(9));
            },
            limit);
    }
    public void receipt(DeliveryApi.Receipt in) {
        if (in == null || in.orderId() == null || !in.orderId().matches("[0-9a-f-]{36}") || in.tradeOrderId() == null
            || !in.tradeOrderId().matches("[1-9][0-9]{0,19}") || in.proof() == null
            || !in.proof().matches("[0-9a-f]{64}")
            || !Set.of("DELIVERED", "REFUND_PENDING", "MANUAL").contains(in.state()))
            throw new CommerceException(400, "交付持久回执非法");
        tx.executeWithoutResult(status -> {
            var rows = jdbc.queryForList(
                "SELECT status FROM commerce_delivery_intent WHERE order_sn=? FOR UPDATE", in.orderId());
            if (rows.size() != 1)
                throw new CommerceException(404, "交付意图不存在");
            var existing = jdbc.queryForList(
                "SELECT trade_order_id,state,proof FROM commerce_delivery_receipt WHERE order_sn=?", in.orderId());
            if (!existing.isEmpty()) {
                var old = existing.getFirst();
                if (!in.tradeOrderId().equals(old.get("trade_order_id")) || !in.state().equals(old.get("state"))
                    || !in.proof().equals(old.get("proof")))
                    throw new CommerceException(409, "同一交付意图的终局发生冲突");
                return;
            }
            jdbc.update("INSERT INTO commerce_delivery_receipt(order_sn,trade_order_id,state,proof) VALUES(?,?,?,?)",
                in.orderId(), in.tradeOrderId(), in.state(), in.proof());
            jdbc.update("UPDATE commerce_delivery_intent SET status=? WHERE order_sn=? AND status='PENDING'",
                in.state(), in.orderId());
            if (in.state().equals("DELIVERED"))
                jdbc.update("UPDATE oms_order SET status=2 WHERE order_sn=? AND status=1", in.orderId());
            if (in.state().equals("MANUAL"))
                jdbc.update("UPDATE oms_order SET status=7 WHERE order_sn=? AND status=1", in.orderId());
            if (in.state().equals("REFUND_PENDING")) {
                jdbc.update("INSERT INTO commerce_refund_intent(order_sn,status) VALUES(?,'PENDING')", in.orderId());
                jdbc.update("UPDATE oms_order SET status=5 WHERE order_sn=? AND status=1", in.orderId());
            }
        });
    }
    public List<CommerceApi.Payment> refunds() {
        return jdbc.query("SELECT r.order_sn,p.channel,p.transaction_id,p.amount_minor,p.currency FROM "
                + "commerce_refund_intent r JOIN commerce_payment_receipt p ON p.order_sn=r.order_sn WHERE "
                + "r.status='PENDING' ORDER BY r.order_sn LIMIT 100",
            (r, n)
                -> new CommerceApi.Payment(
                    r.getString(1), r.getString(2), r.getString(3), r.getLong(4), r.getString(5)));
    }
    public void refunded(CommerceApi.Payment in) {
        if (in == null || in.orderId() == null || in.channel() == null || in.transactionId() == null
            || !in.channel().matches("[a-z][a-z0-9_-]{1,31}") || !in.transactionId().matches("[A-Za-z0-9_-]{1,128}"))
            throw new CommerceException(400, "退款回执非法");
        tx.executeWithoutResult(status -> {
            var original = jdbc.queryForList("SELECT o.status,p.amount_minor,p.currency FROM oms_order o JOIN "
                    + "commerce_payment_receipt p ON p.order_sn=o.order_sn WHERE o.order_sn=? FOR UPDATE",
                in.orderId());
            if (original.size() != 1)
                throw new CommerceException(404, "退款订单不存在");
            var row = original.getFirst();
            if (((Number) row.get("amount_minor")).longValue() != in.amountMinor()
                || !row.get("currency").equals(in.currency()))
                throw new CommerceException(409, "退款金额币种不符");
            var prior = jdbc.queryForList(
                "SELECT channel,transaction_id FROM commerce_refund_receipt WHERE order_sn=?", in.orderId());
            if (!prior.isEmpty()) {
                if (!in.channel().equals(prior.getFirst().get("channel"))
                    || !in.transactionId().equals(prior.getFirst().get("transaction_id")))
                    throw new CommerceException(409, "退款流水冲突");
                return;
            }
            if (((Number) row.get("status")).intValue() != 5)
                throw new CommerceException(409, "订单不在退款中");
            jdbc.update("INSERT INTO commerce_refund_receipt(order_sn,channel,transaction_id,amount_minor,currency) "
                    + "VALUES(?,?,?,?,?)",
                in.orderId(), in.channel(), in.transactionId(), in.amountMinor(), in.currency());
            jdbc.update("UPDATE commerce_refund_intent SET status='REFUNDED' WHERE order_sn=? AND status='PENDING'",
                in.orderId());
            jdbc.update("UPDATE oms_order SET status=6 WHERE order_sn=? AND status=5", in.orderId());
        });
    }
}
