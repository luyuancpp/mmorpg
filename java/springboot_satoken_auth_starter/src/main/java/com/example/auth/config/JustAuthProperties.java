package com.example.auth.config;

import org.springframework.boot.context.properties.ConfigurationProperties;
import org.springframework.context.annotation.Configuration;

/**
 * JustAuth provider configuration.
 *
 * Property keys (application.properties / env):
 *   justauth.github.client-id / client-secret / redirect-uri
 *   justauth.wechat.client-id / client-secret / redirect-uri  (WeChat Open Platform)
 *   justauth.qq.client-id     / client-secret / redirect-uri  (QQ Connect)
 *
 * Each block is optional — providers with missing client-id are not registered
 * in {@link com.example.auth.controller.AuthController#getAuthRequest(String)}.
 *
 * In production prefer environment-variable injection, e.g.:
 *   JUSTAUTH_WECHAT_CLIENT_ID=wxxxxxx
 *   JUSTAUTH_WECHAT_CLIENT_SECRET=xxxxxxxx
 */
@Configuration
@ConfigurationProperties(prefix = "justauth")
public class JustAuthProperties {

    private ProviderConfig github = new ProviderConfig();
    private ProviderConfig wechat = new ProviderConfig();
    private ProviderConfig qq = new ProviderConfig();

    public ProviderConfig getGithub() { return github; }
    public void setGithub(ProviderConfig github) { this.github = github; }

    public ProviderConfig getWechat() { return wechat; }
    public void setWechat(ProviderConfig wechat) { this.wechat = wechat; }

    public ProviderConfig getQq() { return qq; }
    public void setQq(ProviderConfig qq) { this.qq = qq; }

    public static class ProviderConfig {
        private String clientId;
        private String clientSecret;
        private String redirectUri;
        /** Request unionid when available (WeChat / QQ). */
        private boolean unionId = true;

        public String getClientId() { return clientId; }
        public void setClientId(String clientId) { this.clientId = clientId; }

        public String getClientSecret() { return clientSecret; }
        public void setClientSecret(String clientSecret) { this.clientSecret = clientSecret; }

        public String getRedirectUri() { return redirectUri; }
        public void setRedirectUri(String redirectUri) { this.redirectUri = redirectUri; }

        public boolean isUnionId() { return unionId; }
        public void setUnionId(boolean unionId) { this.unionId = unionId; }

        public boolean isConfigured() {
            return clientId != null && !clientId.isBlank()
                    && clientSecret != null && !clientSecret.isBlank();
        }
    }
}
