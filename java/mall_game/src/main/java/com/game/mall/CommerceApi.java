package com.game.mall;

import jakarta.validation.constraints.*;

public final class CommerceApi {
    private CommerceApi() {}
    public record CreateOrder(@NotBlank @Pattern(regexp="[A-Za-z0-9_-]{8,128}") String requestId,
                              @Positive long skuId, @Min(1) @Max(10000) int quantity) {}
    // 此消息只能来自已经核实真实渠道交易的可信适配器；本模块不接受客户端宣称支付成功。
    public record Payment(@NotBlank @Pattern(regexp="[0-9a-f-]{36}") String orderId,
                          @NotBlank @Pattern(regexp="[a-z][a-z0-9_-]{1,31}") String channel,
                          @NotBlank @Pattern(regexp="[A-Za-z0-9_-]{1,128}") String transactionId,
                          @Positive long amountMinor,
                          @NotBlank @Pattern(regexp="[A-Z][A-Z0-9_]{2,15}") String currency) {}
    public record Order(String orderId, String buyerId, long skuId, int quantity, long amountMinor,
                        String currency, String state, long paymentDeadlineMs) {}
}
