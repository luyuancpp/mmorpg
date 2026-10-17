package com.game.mall;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.charset.StandardCharsets;
import java.time.Duration;
import java.util.Map;
import java.util.UUID;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.test.web.server.LocalServerPort;
import org.springframework.jdbc.core.JdbcTemplate;

/** 通过真实 HTTP 与实际 Spring 装配验收订单切片；H2 不能代替 TiDB 集成证据。 */
@SpringBootTest(webEnvironment = SpringBootTest.WebEnvironment.RANDOM_PORT, properties = {
        "server.address=127.0.0.1",
        "spring.datasource.url=jdbc:h2:mem:commerce_http_flow;MODE=MySQL;DB_CLOSE_DELAY=-1;LOCK_TIMEOUT=5000",
        "spring.datasource.username=sa",
        "spring.datasource.password=",
        "spring.sql.init.mode=always",
        "spring.sql.init.schema-locations=classpath:db/schema.sql",
        "commerce.require-tidb=false",
        "commerce.run-mode=test",
        "commerce.gateway-secret=http-flow-gateway-test-only-key-0001",
        "commerce.payment-secret=http-flow-payment-test-only-key-0002"
})
class CommerceHttpFlowTest {
    private static final String BUYER = "18446744073709551615";
    private static final String GATEWAY_KEY = "http-flow-gateway-test-only-key-0001";
    private static final String PAYMENT_KEY = "http-flow-payment-test-only-key-0002";

    @LocalServerPort int port;
    @Value("${server.servlet.context-path:}") String contextPath;
    @Value("${spring.mvc.servlet.path:}") String servletPath;
    @Autowired JdbcTemplate jdbc;
    @Autowired ObjectMapper json;
    private final HttpClient client = HttpClient.newBuilder().connectTimeout(Duration.ofSeconds(3)).build();

    @Test
    void signedOrdersReserveOnceAuthorizeOwnershipAndStopAtPendingDelivery() throws Exception {
        jdbc.update("INSERT INTO pms_sku_stock(id,product_id,sku_code,price_minor,currency,stock,lock_stock,published) VALUES (701,71,'http-sku',1250,'CNY',4,0,1)");
        jdbc.update("INSERT INTO commerce_sku_grant(sku_id,item_config_id,count_per_unit,title) VALUES(701,10,1,'玄铁')");
        byte[] creation = json.writeValueAsBytes(Map.of("requestId", "http-buyer-request-001", "skuId", 701, "quantity", 2));
        HttpRequest original = signed("POST", "/api/orders", "gateway", BUYER, creation);
        HttpResponse<String> created = send(original);
        assertEquals(200, created.statusCode(), created.body());
        JsonNode order = json.readTree(created.body());
        String orderId = order.get("orderId").asText();
        assertEquals(BUYER, order.get("buyerId").asText());
        assertEquals(2500, order.get("amountMinor").asLong());
        assertEquals("AWAITING_PAYMENT", order.get("state").asText());
        assertTrue(order.get("paymentDeadlineMs").asLong() > System.currentTimeMillis());

        // 原签名不能重放；以新 nonce 重试同业务请求号只返回原订单。
        assertEquals(409, send(original).statusCode());
        HttpResponse<String> replay = call("POST", "/api/orders", "gateway", BUYER, creation);
        assertEquals(200, replay.statusCode(), replay.body());
        assertEquals(orderId, json.readTree(replay.body()).get("orderId").asText());
        assertEquals(1, count("oms_order"));
        assertEquals(2, stockValue("lock_stock"));
        assertEquals(4, stockValue("stock"));
        byte[] changed = json.writeValueAsBytes(Map.of("requestId", "http-buyer-request-001", "skuId", 701, "quantity", 1));
        assertEquals(409, call("POST", "/api/orders", "gateway", BUYER, changed).statusCode());
        assertEquals(404, call("GET", "/api/orders/" + orderId, "gateway", "2", new byte[0]).statusCode());
        assertEquals(401, send(HttpRequest.newBuilder(uri("/api/orders/" + orderId)).GET().build()).statusCode());

        byte[] payment = json.writeValueAsBytes(Map.of("orderId", orderId, "channel", "test-channel", "transactionId", "HttpPayment001", "amountMinor", 2500, "currency", "CNY"));
        assertEquals(401, call("POST", "/internal/payments", "gateway", BUYER, payment).statusCode());
        HttpResponse<String> paid = call("POST", "/internal/payments", "payment", "payment-adapter", payment);
        assertEquals(200, paid.statusCode(), paid.body());
        assertEquals("DELIVERY_PENDING", json.readTree(paid.body()).get("state").asText());
        assertEquals(200, call("POST", "/internal/payments", "payment", "payment-adapter", payment).statusCode());
        assertEquals(1, count("commerce_payment_receipt"));
        assertEquals(1, count("commerce_delivery_intent"));
        assertEquals(2, stockValue("stock"));
        assertEquals(0, stockValue("lock_stock"));
        assertEquals(409, call("POST", "/api/orders/" + orderId + "/cancel", "gateway", BUYER, new byte[0]).statusCode());

        byte[] second = json.writeValueAsBytes(Map.of("requestId", "http-buyer-request-002", "skuId", 701, "quantity", 1));
        HttpResponse<String> secondCreated = call("POST", "/api/orders", "gateway", BUYER, second);
        assertEquals(200, secondCreated.statusCode(), secondCreated.body());
        String secondId = json.readTree(secondCreated.body()).get("orderId").asText();
        assertEquals(1, stockValue("lock_stock"));
        HttpResponse<String> cancelled = call("POST", "/api/orders/" + secondId + "/cancel", "gateway", BUYER, new byte[0]);
        assertEquals(200, cancelled.statusCode(), cancelled.body());
        assertEquals("CANCELLED", json.readTree(cancelled.body()).get("state").asText());
        assertEquals(200, call("POST", "/api/orders/" + secondId + "/cancel", "gateway", BUYER, new byte[0]).statusCode());
        assertEquals(2, stockValue("stock"));
        assertEquals(0, stockValue("lock_stock"));
        assertEquals(1, count("commerce_delivery_intent"));
    }

    private HttpResponse<String> call(String method, String path, String role, String subject, byte[] body) throws Exception {
        return send(signed(method, path, role, subject, body));
    }

    private HttpRequest signed(String method, String path, String role, String subject, byte[] body) {
        String timestamp = Long.toString(System.currentTimeMillis());
        String nonce = UUID.randomUUID().toString();
        String key = role.equals("payment") ? PAYMENT_KEY : GATEWAY_KEY;
        return HttpRequest.newBuilder(uri(path)).timeout(Duration.ofSeconds(8))
                .header("Content-Type", "application/json")
                .header("X-Commerce-Subject", subject)
                .header("X-Commerce-Timestamp", timestamp)
                .header("X-Commerce-Nonce", nonce)
                .header("X-Commerce-Signature", HmacAuthentication.sign(key, role, subject, timestamp, nonce, method, requestPath(path), body))
                .method(method, HttpRequest.BodyPublishers.ofByteArray(body)).build();
    }

    private HttpResponse<String> send(HttpRequest request) throws Exception {
        return client.send(request, HttpResponse.BodyHandlers.ofString(StandardCharsets.UTF_8));
    }

    private URI uri(String path) { return URI.create("http://127.0.0.1:" + port + requestPath(path)); }
    private String requestPath(String path) { return contextPath + ("/".equals(servletPath) ? "" : servletPath) + path; }
    private int count(String table) { return jdbc.queryForObject("SELECT COUNT(*) FROM " + table, Integer.class); }
    private int stockValue(String column) { return jdbc.queryForObject("SELECT " + column + " FROM pms_sku_stock WHERE id=701", Integer.class); }
}
