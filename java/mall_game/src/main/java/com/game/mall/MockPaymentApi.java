package com.game.mall;

/** 仅本地mock适配器的命令；金额、币种和成功事实均不能由调用者声明。 */
public final class MockPaymentApi {
    private MockPaymentApi() {}
    public record Pay(String orderId, String requestId) {}
    // failed表示已尝试的订单失败；scanFailures表示未能安全取出下一项，游标没有越过未知项。
    public record Reconcile(int attempted, int succeeded, int failed, int scanFailures, boolean busy, boolean budgetExhausted) {
        boolean unavailable() { return failed != 0 || scanFailures != 0 || busy; }
    }
}
