package com.game.mall;

import static org.junit.jupiter.api.Assertions.*;

import java.nio.charset.StandardCharsets;
import org.junit.jupiter.api.Test;

/**
 * 商城签名的跨语言黄金样例：同一组输入、同一个期望值逐字出现在
 * go/trade/internal/commerce/mall_signature_test.go、robot/commerce_mock_payer_test.go 与本文件。
 * 期望值由与三边都无关的 Python hmac/hashlib 离线算出。改签名算法必须三边同批改、三处期望值同批换，
 * 不能只把某一边的字面量改成新输出——那样线上表现是「全部 401」。
 */
class HmacSignatureGoldenTest {
    private static final String PAYMENT_BODY = "{\"orderId\":\"00112233-4455-6677-8899-aabbccddeeff\",\"channel\":\"mock-dev\","
        + "\"transactionId\":\"mockdev-00112233445566778899aabbccddeeff\",\"amountMinor\":600,\"currency\":\"CNY\"}";

    @Test void 付款角色带正文的签名与三语言黄金值一致() {
        assertEquals("886584609f3fb1fa50a5ddb30f02837fb7e75d11fa0075f344447872c7eaf051",
            HmacAuthentication.sign("commerce-golden-payment-secret-0123456789", "payment", "payment-adapter", "1700000000123",
                "golden-nonce-0000000001", "POST", "/internal/payments", PAYMENT_BODY.getBytes(StandardCharsets.UTF_8)));
    }

    @Test void 空正文GET的签名取空字节摘要并与三语言黄金值一致() {
        assertEquals("3d85985f791cbcf33107071195269b4a64890a3c3637ada6379df8c1c547fae1",
            HmacAuthentication.sign("commerce-golden-gateway-secret-0123456789", "gateway", "22", "1700000000456",
                "golden-nonce-0000000002", "GET", "/api/orders/00112233-4455-6677-8899-aabbccddeeff", new byte[0]));
    }
}
