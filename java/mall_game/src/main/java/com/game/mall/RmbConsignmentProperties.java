package com.game.mall;

import jakarta.annotation.PostConstruct;
import java.util.Set;
import org.springframework.boot.context.properties.ConfigurationProperties;
import org.springframework.stereotype.Component;

/** 时长单位毫秒、费率单位万分比；首次寄售冻结配置，改配置不改旧单。 */
@Component
@ConfigurationProperties("commerce.consignment")
public final class RmbConsignmentProperties {
    private long noticeMs = 172_800_000;
    private long saleMs = 604_800_000;
    private long coolingMs = 259_200_000;
    private int feeBps = 500;
    private Set<Integer> currencies = Set.of(0);
    public long getNoticeMs() { return noticeMs; }
    public void setNoticeMs(long value) { noticeMs = value; }
    public long getSaleMs() { return saleMs; }
    public void setSaleMs(long value) { saleMs = value; }
    public long getCoolingMs() { return coolingMs; }
    public void setCoolingMs(long value) { coolingMs = value; }
    public int getFeeBps() { return feeBps; }
    public void setFeeBps(int value) { feeBps = value; }
    public Set<Integer> getCurrencies() { return currencies; }
    public void setCurrencies(Set<Integer> value) { currencies = value == null ? null : Set.copyOf(value); }
    @PostConstruct public void validate() {
        long maximum = 366L * 86_400_000;
        if (noticeMs < 0 || noticeMs > maximum || saleMs < 1 || saleMs > maximum || coolingMs < 0 || coolingMs > maximum
            || feeBps < 0 || feeBps > 10_000 || currencies == null
            || currencies.stream().anyMatch(value -> value == null || value < 0 || value > 1)) {
            throw new IllegalStateException("人民币寄售规则非法；绑定货币不能交易");
        }
    }
}
