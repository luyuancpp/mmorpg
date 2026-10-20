package com.example.auth.service;

import cn.dev33.satoken.stp.StpUtil;
import org.springframework.data.redis.core.RedisTemplate;
import org.springframework.stereotype.Component;

import java.time.Duration;
import java.util.LinkedHashMap;
import java.util.Map;

/**
 * 把刚 {@code StpUtil.login} 出的令牌写成 Go login {@code SaTokenProvider} 读取的 Redis 键
 * （{@code <tokenName>:login:token:<tokenValue>} → loginId），并组装回给客户端的响应。
 * OAuth 回调（{@code AuthController}）与 dev profile 的 {@code DevLoginController} 共用。
 */
@Component
public class SaTokenResponses {
    private final RedisTemplate<String, Object> redisTemplate;

    public SaTokenResponses(RedisTemplate<String, Object> redisTemplate) {
        this.redisTemplate = redisTemplate;
    }

    public Map<String, Object> issue(String loginId) {
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
}
