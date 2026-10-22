package com.game.mall;

import jakarta.servlet.http.HttpServletRequest;
import org.springframework.web.bind.annotation.*;

@RestController
public final class CatalogAdminController {
    private final CatalogAdminStore store;
    private final CommerceStore orders;
    public CatalogAdminController(CatalogAdminStore store, CommerceStore orders) { this.store = store; this.orders = orders; }

    @PostMapping("/internal/admin/order-view")
    public CommerceApi.Order order(HttpServletRequest request, @RequestBody CommerceApi.OrderQuery input) {
        actor(request);
        if (input == null) { throw new CommerceException(400, "订单查询参数缺失"); }
        return orders.paymentOrderView(input.orderId());
    }

    @PostMapping("/internal/admin/skus/list")
    public CatalogAdminApi.Skus list(HttpServletRequest request, @RequestBody DeliveryApi.SkuPage input) {
        actor(request);
        return store.list(input);
    }
    @PostMapping("/internal/admin/skus/create")
    public CatalogAdminApi.Sku create(HttpServletRequest request, @RequestBody CatalogAdminApi.Create input) {
        return store.create(actor(request), input);
    }
    @PostMapping("/internal/admin/skus/update")
    public CatalogAdminApi.Sku update(HttpServletRequest request, @RequestBody CatalogAdminApi.Update input) {
        return store.update(actor(request), input);
    }
    @PostMapping("/internal/admin/skus/stock")
    public CatalogAdminApi.Sku stock(HttpServletRequest request, @RequestBody CatalogAdminApi.AdjustStock input) {
        return store.adjustStock(actor(request), input);
    }
    private static String actor(HttpServletRequest request) {
        if (!"admin".equals(request.getAttribute(HmacAuthentication.ROLE))) {
            throw new CommerceException(401, "管理调用方认证失败");
        }
        return (String) request.getAttribute(HmacAuthentication.SUBJECT);
    }
}
