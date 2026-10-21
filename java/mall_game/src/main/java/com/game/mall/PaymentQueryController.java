package com.game.mall;

import jakarta.servlet.http.HttpServletRequest;
import jakarta.validation.Valid;
import org.springframework.web.bind.annotation.*;

/** 渠道适配器只读查询接缝；业务事实仍只来自原 OMS/PMS 和冻结的订单映射。 */
@RestController
public final class PaymentQueryController {
    private final CommerceStore orders;
    private final DeliveryStore catalog;

    public PaymentQueryController(CommerceStore orders, DeliveryStore catalog) {
        this.orders = orders;
        this.catalog = catalog;
    }

    @PostMapping("/internal/payments/order-view")
    public CommerceApi.Order orderView(HttpServletRequest request, @Valid @RequestBody CommerceApi.OrderQuery input) {
        requirePayment(request);
        return orders.paymentOrderView(input.orderId());
    }

    @PostMapping("/internal/payments/skus")
    public DeliveryApi.Skus skus(HttpServletRequest request, @RequestBody DeliveryApi.SkuPage input) {
        requirePayment(request);
        return catalog.skus(input);
    }

    private static void requirePayment(HttpServletRequest request) {
        if (!"payment".equals(request.getAttribute(HmacAuthentication.ROLE))) {
            throw new CommerceException(401, "支付调用方认证失败");
        }
    }
}
