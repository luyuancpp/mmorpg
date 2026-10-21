package com.game.mall;

import java.util.List;

/** 运维命令只修改未来订单条款；不接受支付、交付状态或玩家资产。 */
public final class CatalogAdminApi {
    private CatalogAdminApi() {}

    public record Terms(long productId, String skuCode, long priceMinor, String currency, boolean published,
        int itemConfigId, int countPerUnit, String title) {}
    public record Create(String requestId, long skuId, Terms terms, int stock) {}
    public record Update(String requestId, long skuId, long expectedRevision, Terms terms) {}
    public record AdjustStock(String requestId, long skuId, long expectedRevision, int delta) {}
    public record Sku(long skuId, long revision, long productId, String skuCode, long priceMinor, String currency,
        int stock, int lockedStock, boolean published, int itemConfigId, int countPerUnit, String title) {}
    public record Skus(List<Sku> skus) {}
}
