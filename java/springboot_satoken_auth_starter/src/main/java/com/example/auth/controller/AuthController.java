package com.example.auth.controller;

import cn.dev33.satoken.stp.StpUtil;
import com.example.auth.config.JustAuthProperties;
import com.example.auth.config.JustAuthProperties.ProviderConfig;
import com.example.auth.service.OauthService;
import com.example.auth.service.SaTokenResponses;
import me.zhyd.oauth.config.AuthConfig;
import me.zhyd.oauth.enums.AuthResponseStatus;
import me.zhyd.oauth.model.AuthCallback;
import me.zhyd.oauth.model.AuthResponse;
import me.zhyd.oauth.model.AuthUser;
import me.zhyd.oauth.request.AuthGithubRequest;
import me.zhyd.oauth.request.AuthQqRequest;
import me.zhyd.oauth.request.AuthRequest;
import me.zhyd.oauth.request.AuthWeChatOpenRequest;
import me.zhyd.oauth.utils.AuthStateUtils;
import org.springframework.web.bind.annotation.*;

/**
 * JustAuth OAuth 回调换 SA-Token 的入口（本 starter 是示例应用，没有任何部署清单）。
 *
 * <p>本地联调用的 {@code /auth/dev-login}、{@code /auth/logout} 在 {@link DevLoginController}，只在 dev profile 注册：
 * dev-login 会把任意 account 写进 Go login {@code SaTokenProvider} 信任的 Redis 键，等于「报上账号名即登录」。
 */
@RestController
@RequestMapping("/auth")
public class AuthController {
    private final JustAuthProperties authProps;
    private final OauthService oauthService;
    private final SaTokenResponses tokenResponses;

    public AuthController(JustAuthProperties authProps,
                          OauthService oauthService,
                          SaTokenResponses tokenResponses) {
        this.authProps = authProps;
        this.oauthService = oauthService;
        this.tokenResponses = tokenResponses;
    }

    // Redirect to OAuth authorization page
    @GetMapping("/login/{provider}")
    public String renderAuth(@PathVariable("provider") String provider) {
        AuthRequest authRequest = getAuthRequest(provider);
        return authRequest.authorize(AuthStateUtils.createState());
    }

    @GetMapping("/callback/{provider}")
    public Object login(@PathVariable String provider, AuthCallback callback) {
        AuthRequest authRequest = getAuthRequest(provider);
        AuthResponse response = authRequest.login(callback);

        if (response.getCode() != AuthResponseStatus.SUCCESS.getCode()) {
            return response;
        }

        AuthUser authUser = (AuthUser) response.getData();
        // Prefer unionid (so the same player keeps the same identity across sub-apps
        // under the same WeChat Open Platform / QQ Connect subject). JustAuth puts
        // unionid on AuthToken, while AuthUser.uuid defaults to openid.
        String thirdPartyId = null;
        if (authUser.getToken() != null) {
            thirdPartyId = authUser.getToken().getUnionId();
        }
        if (thirdPartyId == null || thirdPartyId.isBlank()) {
            thirdPartyId = authUser.getUuid();
        }

        // Persist provider+openid -> userId mapping (creates user on first login).
        Long userId = oauthService.findOrCreateAndBind(
                provider,
                thirdPartyId,
                authUser.getNickname(),
                authUser.getAvatar());

        // Login via Sa-Token using the stable internal userId, not the third-party id.
        StpUtil.login(userId);
        return tokenResponses.issue(String.valueOf(userId));
    }

    // Create AuthRequest for the given provider (JustAuth)
    private AuthRequest getAuthRequest(String provider) {
        switch (provider) {
            case "github": {
                ProviderConfig c = requireConfigured(provider, authProps.getGithub());
                return new AuthGithubRequest(buildAuthConfig(c));
            }
            case "wechat":
            case "wechat_open": {
                // WeChat Open Platform (mobile/PC SDK + scan-code login).
                // For 公众号 use AuthWeChatMpRequest instead (different endpoint).
                ProviderConfig c = requireConfigured(provider, authProps.getWechat());
                return new AuthWeChatOpenRequest(buildAuthConfig(c));
            }
            case "qq": {
                ProviderConfig c = requireConfigured(provider, authProps.getQq());
                return new AuthQqRequest(buildAuthConfig(c));
            }
            default:
                throw new RuntimeException("Unsupported provider: " + provider);
        }
    }

    private static ProviderConfig requireConfigured(String name, ProviderConfig c) {
        if (c == null || !c.isConfigured()) {
            throw new RuntimeException("Auth provider not configured: " + name
                    + " (set justauth." + name + ".client-id / client-secret)");
        }
        return c;
    }

    private static AuthConfig buildAuthConfig(ProviderConfig c) {
        return AuthConfig.builder()
                .clientId(c.getClientId())
                .clientSecret(c.getClientSecret())
                .redirectUri(c.getRedirectUri())
                .unionId(c.isUnionId())
                .build();
    }
}
