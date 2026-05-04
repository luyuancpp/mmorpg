package com.example.auth.controller;

import cn.dev33.satoken.stp.StpUtil;
import com.example.auth.config.JustAuthProperties;
import com.example.auth.config.JustAuthProperties.ProviderConfig;
import com.example.auth.dto.LoginDTO;
import com.example.auth.service.OauthService;
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
import org.springframework.data.redis.core.RedisTemplate;
import org.springframework.web.bind.annotation.*;

import java.time.Duration;
import java.util.LinkedHashMap;
import java.util.Map;

@RestController
@RequestMapping("/auth")
public class AuthController {
    private final RedisTemplate<String, Object> redisTemplate;
    private final JustAuthProperties authProps;
    private final OauthService oauthService;

    public AuthController(RedisTemplate<String, Object> redisTemplate,
                          JustAuthProperties authProps,
                          OauthService oauthService) {
        this.redisTemplate = redisTemplate;
        this.authProps = authProps;
        this.oauthService = oauthService;
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
        // Use unionid (when WeChat/QQ provide it) so a player keeps the same
        // identity across sub-apps under the same Open Platform subject.
        // JustAuth fills AuthUser.uuid with unionid when unionId(true) is set
        // and the upstream returns one; otherwise it's openid.
        String thirdPartyId = authUser.getUuid();

        // Persist provider+openid -> userId mapping (creates user on first login).
        Long userId = oauthService.findOrCreateAndBind(
                provider,
                thirdPartyId,
                authUser.getNickname(),
                authUser.getAvatar());

        // Login via Sa-Token using the stable internal userId, not the third-party id.
        StpUtil.login(userId);
        return buildTokenResponse(String.valueOf(userId));
    }

    // Local dev helper: issue a SA-Token directly from an account name.
    // This avoids the OAuth round-trip during local integration testing.
    @GetMapping("/dev-login")
    public Object devLoginByQuery(@RequestParam String account) {
        return issueDevToken(account);
    }

    @PostMapping("/dev-login")
    public Object devLoginByBody(@RequestBody(required = false) LoginDTO body) {
        String account = body == null ? null : body.getAccount();
        return issueDevToken(account);
    }

    private Object issueDevToken(String account) {
        if (account == null || account.isBlank()) {
            return Map.of("ok", false, "message", "account is required");
        }

        String normalized = account.trim();
        StpUtil.login(normalized);
        return buildTokenResponse(normalized);
    }

    // Logout endpoint: removes the manually-written Redis key so a long-running
    // stress test doesn't accumulate stale satoken:login:token:* entries.
    // Accepts either a token value or an account; passing token is preferred
    // because it removes exactly that one key rather than scanning.
    @GetMapping("/logout")
    public Object logout(@RequestParam(required = false) String token,
                         @RequestParam(required = false) String account) {
        String tokenName = StpUtil.getTokenName();

        if (token != null && !token.isBlank()) {
            String redisKey = tokenName + ":login:token:" + token.trim();
            Boolean removed = redisTemplate.delete(redisKey);
            return Map.of(
                    "ok", true,
                    "removed", Boolean.TRUE.equals(removed),
                    "redis_key", redisKey
            );
        }

        if (account != null && !account.isBlank()) {
            // SA-Token can map account -> tokens it knows about, but our manually
            // written keys may include tokens SA-Token already evicted. Delegate
            // to SA-Token's logoutByLoginId for whatever it tracks; the manual
            // key is left to expire via TTL if its token value is unknown.
            try {
                StpUtil.logout(account.trim());
            } catch (Exception e) {
                return Map.of("ok", false, "message", "logout failed: " + e.getMessage());
            }
            return Map.of("ok", true, "account", account.trim());
        }

        return Map.of("ok", false, "message", "token or account required");
    }

    private Map<String, Object> buildTokenResponse(String loginId) {
        String tokenName = StpUtil.getTokenName();
        String tokenValue = StpUtil.getTokenValue();
        String redisKey = tokenName + ":login:token:" + tokenValue;
        redisTemplate.opsForValue().set(redisKey, loginId, Duration.ofDays(7));

        Map<String, Object> result = new LinkedHashMap<>();
        result.put("ok", true);
        result.put("account", loginId);
        result.put("auth_type", "satoken");
        result.put("token_name", tokenName);
        result.put("token_value", tokenValue);
        result.put("login_type", "login");
        result.put("redis_key", redisKey);
        result.put("expires_in_seconds", 604800);
        return result;
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
