package com.game.mall;

import com.fasterxml.jackson.core.JsonParser;
import com.fasterxml.jackson.databind.DeserializationFeature;
import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import jakarta.servlet.http.HttpServletRequest;
import java.io.IOException;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RestController;

@RestController
public final class MockPayoutController {
    private final MockPayoutStore store;
    private final ObjectMapper json;
    public MockPayoutController(MockPayoutStore store, ObjectMapper json) { this.store = store; this.json = json; }
    @PostMapping("/internal/mock/withdraw")
    public RmbConsignmentApi.Withdrawal pay(HttpServletRequest request) {
        JsonNode node = input(request, JsonNode.class);
        if (node == null || !node.isObject() || node.size() != 2
                || !node.path("withdrawalId").isTextual() || !node.path("requestId").isTextual()) {
            throw new CommerceException(400, "mock出款命令只接受两个字符串标识");
        }
        return store.pay(new MockPayoutStore.Pay(node.get("withdrawalId").textValue(), node.get("requestId").textValue()));
    }
    @PostMapping("/internal/mock/withdrawals/reconcile")
    public ResponseEntity<MockPaymentApi.Reconcile> reconcile(HttpServletRequest request) {
        JsonNode node = input(request, JsonNode.class);
        if (node == null || !node.isObject() || !node.isEmpty()) { throw new CommerceException(400, "mock对账命令必须为空对象"); }
        var result = store.reconcile();
        return ResponseEntity.status(result.unavailable() ? 503 : 200).body(result);
    }
    private <T> T input(HttpServletRequest request, Class<T> type) {
        if (!"payment".equals(request.getAttribute(HmacAuthentication.ROLE))) { throw new CommerceException(401, "支付调用方认证失败"); }
        try {
            return json.readerFor(type).with(DeserializationFeature.FAIL_ON_UNKNOWN_PROPERTIES)
                .with(DeserializationFeature.FAIL_ON_TRAILING_TOKENS).with(JsonParser.Feature.STRICT_DUPLICATE_DETECTION)
                .readValue(request.getInputStream());
        } catch (IOException invalid) { throw new CommerceException(400, "mock出款命令非法"); }
    }
}
