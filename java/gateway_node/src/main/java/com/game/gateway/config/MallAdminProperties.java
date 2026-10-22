package com.game.gateway.config;

import jakarta.annotation.PostConstruct;
import java.net.URI;
import java.nio.charset.StandardCharsets;
import org.springframework.boot.context.properties.ConfigurationProperties;
import org.springframework.stereotype.Component;

/** 只由服务端配置目标与商城管理签名，浏览器无权选择代理地址。 */
@Component
@ConfigurationProperties("mall-admin")
public final class MallAdminProperties {
    private String baseUrl = "";
    private String secret = "";
    private String subject = "mall-admin";
    public String getBaseUrl() { return baseUrl; }
    public void setBaseUrl(String value) { baseUrl = value; }
    public String getSecret() { return secret; }
    public void setSecret(String value) { secret = value; }
    public String getSubject() { return subject; }
    public void setSubject(String value) { subject = value; }
    public boolean enabled() { return baseUrl != null && !baseUrl.isEmpty(); }
    @PostConstruct public void validate() {
        if (!enabled()) { return; }
        URI target;
        try { target = URI.create(baseUrl); }
        catch (IllegalArgumentException invalid) { throw new IllegalStateException("商城管理地址非法"); }
        String path = target.getRawPath();
        if (!("http".equals(target.getScheme()) || "https".equals(target.getScheme())) || target.getHost() == null
            || target.getUserInfo() != null || target.getQuery() != null || target.getFragment() != null
            || path.contains("%") || path.contains(";") || path.contains("..") || path.contains("//")) {
            throw new IllegalStateException("商城管理地址必须为无认证信息的固定HTTP地址");
        }
        if (secret == null || secret.getBytes(StandardCharsets.UTF_8).length < 32
            || subject == null || !subject.matches("[a-z][a-z0-9-]{2,63}")) {
            throw new IllegalStateException("商城管理签名配置缺失或非法");
        }
    }
}
