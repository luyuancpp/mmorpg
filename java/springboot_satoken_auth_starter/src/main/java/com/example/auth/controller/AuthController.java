package com.example.auth.controller;

import cn.dev33.satoken.stp.StpUtil;
import com.example.auth.dto.LoginDTO;
import me.zhyd.oauth.config.AuthConfig;
import me.zhyd.oauth.enums.AuthResponseStatus;
import me.zhyd.oauth.model.AuthCallback;
import me.zhyd.oauth.model.AuthResponse;
import me.zhyd.oauth.model.AuthUser;
import me.zhyd.oauth.request.AuthRequest;
import me.zhyd.oauth.utils.AuthStateUtils;
import org.springframework.data.redis.core.RedisTemplate;
import org.springframework.web.bind.annotation.*;

import me.zhyd.oauth.request.AuthGithubRequest;

import java.time.Duration;
import java.util.LinkedHashMap;
import java.util.Map;

@RestController
@RequestMapping("/auth")
public class AuthController {
    private final RedisTemplate<String, Object> redisTemplate;

    public AuthController(RedisTemplate<String, Object> redisTemplate) {
        this.redisTemplate = redisTemplate;
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

        if (response.getCode() == AuthResponseStatus.SUCCESS.getCode()) {

            // Cast response data to AuthUser
            AuthUser authUser = (AuthUser) response.getData();

            // Third-party unique ID per JustAuth convention
            String uuid = authUser.getUuid();

            // Login via Sa-Token (using uuid temporarily; replace with userId later)
            StpUtil.login(uuid);

            return buildTokenResponse(uuid);
        }
        return response;
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
            case "github":
                return new AuthGithubRequest(AuthConfig.builder()
                        .clientId("YOUR_CLIENT_ID")
                        .clientSecret("YOUR_CLIENT_SECRET")
                        .redirectUri("http://localhost:18080/auth/callback/github")
                        .build());
            default:
                throw new RuntimeException("Unsupported provider: " + provider);
        }
    }
}
