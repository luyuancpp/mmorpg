package com.game.mall;

import jakarta.validation.constraints.*;

public final class CommerceApi {
    private CommerceApi() {}
    public record CreateOrder(@NotBlank @Pattern(regexp="[A-Za-z0-9_-]{8,128}") String requestId,
                              @Positive long skuId, @Min(1) @Max(10000) int quantity,
                              @NotNull @Size(min=1,max=191) String buyerAccount, @NotBlank String buyerOwnerRevision) {}
    // 可信trade根据原合同补原修订；不能把过户后当前角色修订冒充原订单身份。
    public record OwnedOrder(String orderId, String buyerAccount, String buyerOwnerRevision) {}
    // 此消息只能来自已核实渠道事实的可信适配器；本地mock还须匹配独立原件，不接受客户端宣称成功。
    public record Payment(@NotBlank @Pattern(regexp="[0-9a-f-]{36}") String orderId,
                          @NotBlank @Pattern(regexp="[a-z][a-z0-9_-]{1,31}") String channel,
                          @NotBlank @Pattern(regexp="[A-Za-z0-9_-]{1,128}") String transactionId,
                          @Positive long amountMinor,
                          @NotBlank @Pattern(regexp="[A-Z][A-Z0-9_]{2,15}") String currency) {}
    public record Order(String orderId, String buyerId, long skuId, int quantity, long amountMinor,
                        String currency, String state, long paymentDeadlineMs) {}
    // 仅供已认证的 payment 角色查原订单；金额与归属均由服务器原记录返回。
    public record OrderQuery(@NotBlank @Size(max=36) String orderId) {}
}
