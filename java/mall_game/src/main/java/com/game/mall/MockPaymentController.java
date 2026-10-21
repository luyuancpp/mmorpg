package com.game.mall;

import com.fasterxml.jackson.databind.DeserializationFeature;
import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.ObjectReader;
import jakarta.servlet.http.HttpServletRequest;
import java.io.IOException;
import java.util.HashSet;
import java.util.Set;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RestController;

/** 双重角色校验与显式字段集合，拒绝调用者传入金额、币种或支付成功声明。 */
@RestController
public final class MockPaymentController {
    private final MockPaymentStore store;
    private final CommerceProperties props;
    private final ObjectReader reader;
    public MockPaymentController(MockPaymentStore store, CommerceProperties props, ObjectMapper json) {
        this.store = store; this.props = props;
        reader = json.readerFor(JsonNode.class).with(DeserializationFeature.FAIL_ON_TRAILING_TOKENS)
            .with(DeserializationFeature.FAIL_ON_READING_DUP_TREE_KEY);
    }
    @PostMapping("/internal/mock/pay")
    public CommerceApi.Order pay(HttpServletRequest request) {
        requirePayment(request);
        var input = document(request, Set.of("orderId", "requestId"));
        if (!input.path("orderId").isTextual() || !input.path("requestId").isTextual()) {
            throw new CommerceException(400, "mock付款字段必须是字符串");
        }
        return store.pay(new MockPaymentApi.Pay(input.get("orderId").textValue(), input.get("requestId").textValue()));
    }
    @PostMapping("/internal/mock/reconcile")
    public ResponseEntity<MockPaymentApi.Reconcile> reconcile(HttpServletRequest request) {
        requirePayment(request);
        document(request, Set.of());
        var result = store.reconcile();
        return ResponseEntity.status(result.unavailable() ? 503 : 200).body(result);
    }
    private void requirePayment(HttpServletRequest request) {
        if (!"payment".equals(request.getAttribute(HmacAuthentication.ROLE))) {
            throw new CommerceException(401, "支付调用方认证失败");
        }
        props.requireMockPayment();
    }
    private JsonNode document(HttpServletRequest request, Set<String> fields) {
        try {
            JsonNode node = reader.readValue(request.getInputStream());
            var actual = new HashSet<String>();
            if (node != null) { node.fieldNames().forEachRemaining(actual::add); }
            if (node == null || !node.isObject() || !actual.equals(fields)) { throw new CommerceException(400, "mock命令字段非法"); }
            return node;
        } catch (IOException invalid) { throw new CommerceException(400, "mock命令必须是完整JSON对象"); }
    }
}
