package com.game.mall;

import jakarta.servlet.http.HttpServletRequest;
import jakarta.validation.Valid;
import java.util.Map;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

@RestController
public class CommerceController {
    private final CommerceStore store;
    public CommerceController(CommerceStore store) { this.store = store; }
    @PostMapping("/api/orders") public CommerceApi.Order create(HttpServletRequest request, @Valid @RequestBody CommerceApi.CreateOrder body) {
        return store.create(subject(request), body);
    }
    @GetMapping("/api/orders/{orderId}") public CommerceApi.Order get(HttpServletRequest request, @PathVariable("orderId") String orderId) { return store.get(subject(request), orderId); }
    @PostMapping("/api/orders/list") public java.util.List<CommerceApi.Order> list(HttpServletRequest request,@RequestBody DeliveryApi.OrderPage page){return store.list(subject(request),page);}
    @PostMapping("/api/orders/{orderId}/cancel") public CommerceApi.Order cancel(HttpServletRequest request, @PathVariable("orderId") String orderId) { return store.cancel(subject(request), orderId); }
    @PostMapping("/internal/payments") public CommerceApi.Order payment(HttpServletRequest request, @Valid @RequestBody CommerceApi.Payment body) {
        // 经济权限在提交入口再次检查，不能让容器路由前缀或未来路径适配改变信任边界。
        if (!"payment".equals(request.getAttribute(HmacAuthentication.ROLE))) { throw new CommerceException(401, "支付调用方认证失败"); }
        return store.recordTrustedPayment(body);
    }
    private String subject(HttpServletRequest request) { return (String) request.getAttribute(HmacAuthentication.SUBJECT); }

    @RestControllerAdvice
    static class Errors {
        @ExceptionHandler(CommerceException.class) ResponseEntity<Map<String, String>> business(CommerceException error) {
            return ResponseEntity.status(error.status()).body(Map.of("error", error.getMessage()));
        }
    }
}
