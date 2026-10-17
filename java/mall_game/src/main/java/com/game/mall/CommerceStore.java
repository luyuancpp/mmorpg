package com.game.mall;

import com.macro.mall.model.OmsOrder;
import com.macro.mall.model.PmsSkuStock;
import java.math.BigDecimal;
import java.nio.charset.StandardCharsets;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.time.Clock;
import java.util.*;
import org.springframework.dao.DuplicateKeyException;
import org.springframework.context.annotation.DependsOn;
import org.springframework.jdbc.core.JdbcTemplate;
import org.springframework.stereotype.Repository;
import org.springframework.transaction.PlatformTransactionManager;
import org.springframework.transaction.TransactionDefinition;
import org.springframework.transaction.support.TransactionTemplate;

@Repository
@DependsOn("commerceSchemaReady")
public class CommerceStore {
    static final long MAX_AMOUNT_MINOR = 9_000_000_000_000_000L;
    private final JdbcTemplate jdbc;
    private final TransactionTemplate tx;
    private final Clock clock;
    private final int maxUnpaid;
    public CommerceStore(JdbcTemplate jdbc, PlatformTransactionManager manager, Clock clock, CommerceProperties props) {
        this.jdbc = jdbc; this.clock = clock;
        maxUnpaid = props.getMaxUnpaidOrders();
        tx = new TransactionTemplate(manager); tx.setIsolationLevel(TransactionDefinition.ISOLATION_READ_COMMITTED); tx.setTimeout(5);
        jdbc.setQueryTimeout(5);
    }

    public CommerceApi.Order create(String buyer, CommerceApi.CreateOrder in) {
        HmacAuthentication.requirePlayer(buyer);
        if (in.requestId() == null || !in.requestId().matches("[A-Za-z0-9_-]{8,128}") || in.skuId() <= 0 || in.quantity() < 1 || in.quantity() > 10000) {
            throw new CommerceException(400, "下单参数非法");
        }
        Stored prior = byRequest(buyer, in.requestId());
        if (prior != null) { return sameRequest(prior, in); }
        // 首次建 guard 为单条独立提交，行从不删除；没有全服共享锁行。
        jdbc.update("INSERT INTO commerce_buyer_guard(buyer_id) VALUES (?) ON DUPLICATE KEY UPDATE buyer_id=buyer_id", buyer);
        try {
            return Objects.requireNonNull(tx.execute(status -> {
                jdbc.queryForObject("SELECT buyer_id FROM commerce_buyer_guard WHERE buyer_id=? FOR UPDATE", String.class, buyer);
                Stored replay = byRequest(buyer, in.requestId());
                if (replay != null) { return sameRequest(replay, in); }
                Integer unpaid = jdbc.queryForObject("SELECT COUNT(*) FROM oms_order WHERE buyer_id=? AND status=0", Integer.class, buyer);
                if (unpaid == null || unpaid >= maxUnpaid) { throw new CommerceException(429, "未支付订单数量已达上限，请先处理已有订单"); }
                List<Sku> values = jdbc.query("SELECT * FROM pms_sku_stock WHERE id=? FOR UPDATE", (rs, row) -> sku(rs), in.skuId());
                if (values.isEmpty()) { throw new CommerceException(404, "商品不存在"); }
                Sku sku = values.get(0);
                // 同 SKU 竞争者可能刚提交相同请求；当前读隔离下重新复核，原价订单不会被新价格覆盖。
                Stored existing = byRequest(buyer, in.requestId());
                if (existing != null) { return sameRequest(existing, in); }
                PmsSkuStock model = sku.model();
                if (!sku.published() || model.getStock() < 0 || model.getLockStock() < 0 || model.getLockStock() > model.getStock()) {
                    throw new CommerceException(409, "商品未开放或库存状态异常");
                }
                long unitPrice = model.getPrice().longValueExact();
                long total;
                try { total = Math.multiplyExact(unitPrice, in.quantity()); }
                catch (ArithmeticException overflow) { throw new CommerceException(409, "商品金额超限"); }
                if (unitPrice <= 0 || total <= 0 || total > MAX_AMOUNT_MINOR || !sku.currency().matches("[A-Z][A-Z0-9_]{2,15}")) {
                    throw new CommerceException(409, "商品金额或币种非法");
                }
                String orderId = UUID.randomUUID().toString();
                long now = clock.millis();
                // 15 分钟支付窗口供渠道适配器使用；超时仍保留预占，未接关单确认前不自动释放。
                jdbc.update("INSERT INTO oms_order(order_sn,buyer_id,request_id,sku_id,quantity,pay_amount,currency,status,create_time_ms,payment_deadline_ms) VALUES (?,?,?,?,?,?,?,0,?,?)",
                        orderId, buyer, ascii(in.requestId()), in.skuId(), in.quantity(), total, sku.currency(), now, now + 900_000);
                // 改自上游库存双计数：持主键行锁，预占条件在写入时原子复核，绝不先读后绝对值覆盖。
                int changed = jdbc.update("UPDATE pms_sku_stock SET lock_stock=lock_stock+? WHERE id=? AND published=1 AND stock-lock_stock>=?",
                        in.quantity(), in.skuId(), in.quantity());
                if (changed != 1) { throw new CommerceException(409, "库存不足"); }
                jdbc.update("INSERT INTO oms_order_item(order_sn,product_id,product_sku_id,product_sku_code,product_quantity,product_price_minor) VALUES (?,?,?,?,?,?)",
                        orderId, model.getProductId(), in.skuId(), model.getSkuCode(), in.quantity(), unitPrice);
                if(jdbc.update("INSERT INTO commerce_order_grant(order_sn,item_config_id,count_per_unit,title) SELECT ?,item_config_id,count_per_unit,title FROM commerce_sku_grant WHERE sku_id=? AND item_config_id>0 AND count_per_unit>0 AND count_per_unit*?<=4096 AND CHAR_LENGTH(TRIM(title))>0",orderId,in.skuId(),in.quantity())!=1)throw new CommerceException(409,"SKU没有合法游戏交付映射或单次数量越界");
                return view(Objects.requireNonNull(byId(orderId, false)));
            }));
        } catch (DuplicateKeyException conflict) {
            // 跨 SKU 的同幂等键竞争由唯一约束裁决；失败事务先回滚，再返回已提交的同一结果。
            Stored existing = byRequest(buyer, in.requestId());
            if (existing != null) { return sameRequest(existing, in); }
            throw new CommerceException(409, "请求冲突，请使用原请求号重试");
        }
    }

    public CommerceApi.Order get(String buyer, String orderId) {
        HmacAuthentication.requirePlayer(buyer); requireOrderId(orderId);
        return view(owned(byId(orderId, false), buyer));
    }
    public List<CommerceApi.Order> list(String buyer,DeliveryApi.OrderPage page){
        HmacAuthentication.requirePlayer(buyer);if(page==null||page.limit()<1||page.limit()>100||page.afterOrderId()==null)throw new CommerceException(400,"订单分页非法");
        long after=0;if(!page.afterOrderId().isEmpty()){requireOrderId(page.afterOrderId());after=owned(byId(page.afterOrderId(),false),buyer).model().getId();}
        return jdbc.query("SELECT * FROM oms_order WHERE buyer_id=? AND id>? ORDER BY id LIMIT ?",(row,n)->view(stored(row)),buyer,after,page.limit());
    }

    public CommerceApi.Order cancel(String buyer, String orderId) {
        HmacAuthentication.requirePlayer(buyer); requireOrderId(orderId);
        return Objects.requireNonNull(tx.execute(status -> {
            Stored order = owned(byId(orderId, true), buyer);
            if (order.model().getStatus() == 4) { return view(order); }
            if (order.model().getStatus() != 0) { throw new CommerceException(409, "已支付订单不能直接取消"); }
            int released = jdbc.update("UPDATE pms_sku_stock SET lock_stock=lock_stock-? WHERE id=? AND lock_stock>=?",
                    order.quantity(), order.skuId(), order.quantity());
            if (released != 1) { throw new CommerceException(409, "库存预占状态异常"); }
            int changed = jdbc.update("UPDATE oms_order SET status=4 WHERE order_sn=? AND status=0", orderId);
            if (changed != 1) { throw new CommerceException(409, "订单状态冲突"); }
            order.model().setStatus(4); return view(order);
        }));
    }

    public CommerceApi.Order recordTrustedPayment(CommerceApi.Payment in) {
        requireOrderId(in.orderId());
        if (in.channel() == null || !in.channel().matches("[a-z][a-z0-9_-]{1,31}") || in.transactionId() == null || !in.transactionId().matches("[A-Za-z0-9_-]{1,128}") || in.amountMinor() <= 0 || in.amountMinor() > MAX_AMOUNT_MINOR || in.currency() == null) {
            throw new CommerceException(400, "支付回执参数非法");
        }
        try {
            return Objects.requireNonNull(tx.execute(status -> {
                Stored order = byId(in.orderId(), true);
                if (order == null) { throw new CommerceException(404, "订单不存在"); }
                if (order.model().getPayAmount().longValueExact() != in.amountMinor() || !order.currency().equals(in.currency())) {
                    throw new CommerceException(409, "支付金额或币种不匹配");
                }
                if (java.util.Set.of(1,2,5,6,7).contains(order.model().getStatus())) {
                    Integer matches = jdbc.queryForObject("SELECT COUNT(*) FROM commerce_payment_receipt WHERE order_sn=? AND channel=? AND transaction_id=? AND amount_minor=? AND currency=?", Integer.class,
                            in.orderId(), ascii(in.channel()), ascii(in.transactionId()), in.amountMinor(), in.currency());
                    if (matches != null && matches == 1) { return view(order); }
                    throw new CommerceException(409, "该订单已有不同支付回执");
                }
                if (order.model().getStatus() == 4) {
                    jdbc.update("INSERT INTO commerce_payment_receipt(channel,transaction_id,order_sn,amount_minor,currency) VALUES(?,?,?,?,?)",ascii(in.channel()),ascii(in.transactionId()),in.orderId(),in.amountMinor(),in.currency());
                    jdbc.update("INSERT INTO commerce_refund_intent(order_sn,status) VALUES(?,'PENDING')",in.orderId());
                    jdbc.update("UPDATE oms_order SET status=5 WHERE order_sn=? AND status=4",in.orderId());order.model().setStatus(5);return view(order);
                }
                if (order.model().getStatus() != 0) { throw new CommerceException(409, "订单状态冲突"); }
                jdbc.update("INSERT INTO commerce_payment_receipt(channel,transaction_id,order_sn,amount_minor,currency) VALUES (?,?,?,?,?)",
                        ascii(in.channel()), ascii(in.transactionId()), in.orderId(), in.amountMinor(), in.currency());
                // 复用上游 stock/lock_stock 一并扣减结构；补足状态 CAS、幂等收据及非负条件。
                int deducted = jdbc.update("UPDATE pms_sku_stock SET stock=stock-?,lock_stock=lock_stock-? WHERE id=? AND stock>=? AND lock_stock>=?",
                        order.quantity(), order.quantity(), order.skuId(), order.quantity(), order.quantity());
                if (deducted != 1) { throw new CommerceException(409, "库存预占状态异常"); }
                int changed = jdbc.update("UPDATE oms_order SET status=1,payment_time_ms=? WHERE order_sn=? AND status=0", clock.millis(), in.orderId());
                if (changed != 1) { throw new CommerceException(409, "订单状态冲突"); }
                jdbc.update("INSERT INTO commerce_delivery_intent(order_sn,buyer_id,sku_id,quantity,status,created_ms) VALUES (?,?,?,?,?,?)",
                        in.orderId(), order.buyer(), order.skuId(), order.quantity(), "PENDING", clock.millis());
                // 此提交只确认支付和交付意图；没有可信游戏资产持久回执，永不迁移到完成。
                order.model().setStatus(1); return view(order);
            }));
        } catch (DuplicateKeyException conflict) { throw new CommerceException(409, "支付流水已用于其他订单"); }
    }

    private Stored byRequest(String buyer, String requestId) {
        List<Stored> rows = jdbc.query("SELECT * FROM oms_order WHERE buyer_id=? AND request_id=?", (rs, row) -> stored(rs), buyer, ascii(requestId));
        return rows.isEmpty() ? null : rows.get(0);
    }
    private Stored byId(String id, boolean lock) {
        List<Stored> rows = jdbc.query("SELECT * FROM oms_order WHERE order_sn=?" + (lock ? " FOR UPDATE" : ""), (rs, row) -> stored(rs), id);
        return rows.isEmpty() ? null : rows.get(0);
    }
    private Stored owned(Stored order, String buyer) {
        if (order == null || !order.buyer().equals(buyer)) { throw new CommerceException(404, "订单不存在"); }
        return order;
    }
    private CommerceApi.Order sameRequest(Stored prior, CommerceApi.CreateOrder in) {
        if (prior.skuId() != in.skuId() || prior.quantity() != in.quantity()) { throw new CommerceException(409, "请求号已用于不同下单参数"); }
        return view(prior);
    }
    private CommerceApi.Order view(Stored value) {
        String state = switch (value.model().getStatus()) { case 0 -> "AWAITING_PAYMENT"; case 1 -> "DELIVERY_PENDING"; case 2 -> "DELIVERED"; case 4 -> "CANCELLED"; case 5 -> "REFUND_PENDING"; case 6 -> "REFUNDED"; case 7 -> "MANUAL"; default -> throw new CommerceException(503, "订单状态无法处理"); };
        return new CommerceApi.Order(value.model().getOrderSn(), value.buyer(), value.skuId(), value.quantity(), value.model().getPayAmount().longValueExact(), value.currency(), state, value.deadline());
    }
    private static void requireOrderId(String id) {
        if (id == null || !id.matches("[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}")) { throw new CommerceException(400, "订单号非法"); }
    }
    private Sku sku(ResultSet rs) throws SQLException {
        PmsSkuStock value = new PmsSkuStock(); value.setId(rs.getLong("id")); value.setProductId(rs.getLong("product_id"));
        value.setSkuCode(rs.getString("sku_code")); value.setStock(rs.getInt("stock")); value.setLockStock(rs.getInt("lock_stock")); value.setPrice(BigDecimal.valueOf(rs.getLong("price_minor")));
        return new Sku(value, rs.getString("currency"), rs.getInt("published") == 1);
    }
    private Stored stored(ResultSet rs) throws SQLException {
        OmsOrder value = new OmsOrder(); value.setId(rs.getLong("id")); value.setOrderSn(rs.getString("order_sn"));
        value.setPayAmount(BigDecimal.valueOf(rs.getLong("pay_amount"))); value.setStatus(rs.getInt("status")); value.setCreateTime(new Date(rs.getLong("create_time_ms")));
        return new Stored(value, rs.getString("buyer_id"), rs.getLong("sku_id"), rs.getInt("quantity"), rs.getString("currency"), rs.getLong("payment_deadline_ms"));
    }
    private record Sku(PmsSkuStock model, String currency, boolean published) {}
    private record Stored(OmsOrder model, String buyer, long skuId, int quantity, String currency, long deadline) {}
    private static byte[] ascii(String value) { return value.getBytes(StandardCharsets.US_ASCII); }
}
