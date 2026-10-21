package com.game.mall;
import java.util.List;
/** 内部游戏交付契约；付款事实仍只来自渠道认证面，资产证明仅由trade提交。 */
public final class DeliveryApi {
    private DeliveryApi() {}
    public record Poll(int limit, String cursor) {
        public Poll(int limit) { this(limit, ""); }
    }
    // 扫描位置只放包装里；Intent 是已有订单的幂等摘要输入，不能因翻页增加字段。
    public record PollEntry(Intent intent, String cursor) {}
    public record PollPage(List<PollEntry> entries) {}
    public record Intent(String orderId, String buyerId, String buyerAccount, String buyerOwnerRevision,
        long skuId, int quantity, int itemConfigId, int itemCount,
        String title, long amountMinor, String currency) {}
    public record OrderPage(String afterOrderId, int limit, String buyerAccount, String buyerOwnerRevision) {}
    public record SkuPage(long afterSkuId, int limit) {}
    public record Receipt(String orderId, String tradeOrderId, String state, String proof) {}
    public record Sku(long skuId, String title, long priceMinor, String currency, long available) {}
    public record Skus(List<Sku> skus) {}
}
