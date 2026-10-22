package com.game.mall;

import static org.junit.jupiter.api.Assertions.*;

import org.junit.jupiter.api.Test;
import org.springframework.mock.web.MockHttpServletRequest;

/** 控制器再次核验经济权限，防止未来容器转发或过滤器映射绕过。 */
class PaymentQueryControllerTest {
    @Test void 支付查询没有支付角色时整体拒绝且不访问业务存储() {
        var controller = new PaymentQueryController(null, null);
        for (String role : new String[] {null, "gateway", "delivery"}) {
            var request = new MockHttpServletRequest();
            request.setAttribute(HmacAuthentication.ROLE, role);
            assertEquals(401, assertThrows(CommerceException.class,
                () -> controller.orderView(request, new CommerceApi.OrderQuery(null))).status());
            assertEquals(401, assertThrows(CommerceException.class,
                () -> controller.skus(request, new DeliveryApi.SkuPage(0, 20))).status());
        }
    }

    @Test void 支付角色查询原订单和有界SKU页_不写支付或交付事实() {
        try (var f = new CommerceFixture()) {
            f.sku(1, 6, 125);
            f.sku(2, 4, 250);
            var original = f.store.create("7", new CommerceApi.CreateOrder("payment-query-controller", 1, 2, "测试账号", "1"));
            var controller = new PaymentQueryController(f.store, new DeliveryStore(f.jdbc, f.manager, f.props));
            var request = new MockHttpServletRequest();
            request.setAttribute(HmacAuthentication.ROLE, "payment");
            assertEquals(original, controller.orderView(request, new CommerceApi.OrderQuery(original.orderId())));
            var first = controller.skus(request, new DeliveryApi.SkuPage(0, 1));
            assertEquals(1, first.skus().size());
            assertEquals(1, first.skus().getFirst().skuId());
            assertEquals(4, first.skus().getFirst().available());
            assertEquals(2, controller.skus(request, new DeliveryApi.SkuPage(1, 1)).skus().getFirst().skuId());
            assertTrue(controller.skus(request, new DeliveryApi.SkuPage(2, 1)).skus().isEmpty());
            assertEquals(400, assertThrows(CommerceException.class,
                () -> controller.skus(request, new DeliveryApi.SkuPage(-1, 1))).status());
            assertEquals(400, assertThrows(CommerceException.class,
                () -> controller.skus(request, new DeliveryApi.SkuPage(0, 101))).status());
            assertEquals(1, f.count("oms_order"));
            assertEquals(0, f.count("commerce_payment_receipt"));
            assertEquals(0, f.count("commerce_delivery_intent"));
        }
    }
}
