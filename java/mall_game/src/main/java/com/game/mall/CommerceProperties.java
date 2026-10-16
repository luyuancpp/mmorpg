package com.game.mall;

import jakarta.annotation.PostConstruct;
import java.nio.charset.StandardCharsets;
import org.springframework.boot.context.properties.ConfigurationProperties;

@ConfigurationProperties("commerce")
public final class CommerceProperties {
    private boolean requireTidb = true;
    private String runMode = "prod";
    private String gatewaySecret;
    private String paymentSecret;
    private String paymentSubject = "payment-adapter";
    private int maxUnpaidOrders = 5;
    public boolean isRequireTidb() { return requireTidb; }
    public void setRequireTidb(boolean value) { requireTidb = value; }
    public String getRunMode() { return runMode; }
    public void setRunMode(String value) { runMode = value; }
    public String getGatewaySecret() { return gatewaySecret; }
    public void setGatewaySecret(String value) { gatewaySecret = value; }
    public String getPaymentSecret() { return paymentSecret; }
    public void setPaymentSecret(String value) { paymentSecret = value; }
    public String getPaymentSubject() { return paymentSubject; }
    public void setPaymentSubject(String value) { paymentSubject = value; }
    public int getMaxUnpaidOrders() { return maxUnpaidOrders; }
    public void setMaxUnpaidOrders(int value) { maxUnpaidOrders = value; }
    @PostConstruct public void validate() {
        if (!java.util.Set.of("prod", "dev", "test").contains(runMode) || (runMode.equals("prod") && !requireTidb)) {
            throw new IllegalStateException("商城运行模式非法，prod 不允许关闭 TiDB 权威存储要求");
        }
        if (!strong(gatewaySecret) || !strong(paymentSecret) || gatewaySecret.equals(paymentSecret)) {
            throw new IllegalStateException("必须配置两把不同且至少 32 字节的商城调用密钥");
        }
        if (paymentSubject == null || !paymentSubject.matches("[a-z][a-z0-9-]{2,63}")) {
            throw new IllegalStateException("支付调用方名称非法");
        }
        if (maxUnpaidOrders < 1 || maxUnpaidOrders > 100) { throw new IllegalStateException("未支付订单上限必须在 1..100"); }
    }
    private boolean strong(String value) { return value != null && value.getBytes(StandardCharsets.UTF_8).length >= 32; }
}
