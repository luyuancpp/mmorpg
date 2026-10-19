package com.example.auth.controller;

import cn.dev33.satoken.stp.StpUtil;
import com.example.auth.dto.LoginDTO;
import com.example.auth.service.SaTokenResponses;
import org.springframework.context.annotation.Profile;
import org.springframework.data.redis.core.RedisTemplate;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;

import java.util.Map;

/**
 * 本地联调 / 压测专用：免 OAuth 直接按账号名签 SA-Token，以及压测结束清理手写的 Redis 键。
 *
 * <p><b>只在 Spring profile {@code dev} 下注册</b>（{@code dev.bat start-satoken} 以
 * {@code -Dspring-boot.run.profiles=dev} 启动）。没有这道门时，任何能访问本服务的人报上任意 account，
 * 就能拿到 Go login {@code SaTokenProvider} 信任的令牌，以该账号登录游戏；按 account 的 logout 也能把别人踢下线。
 * 生产的第三方登录只走 Go login 直连微信 / QQ（auth_type=wechat / qq），不经过本服务
 * （docs/design/third-party-login-end-to-end-design.md）。
 */
@RestController
@RequestMapping("/auth")
@Profile("dev")
public class DevLoginController {
    private final RedisTemplate<String, Object> redisTemplate;
    private final SaTokenResponses tokenResponses;

    public DevLoginController(RedisTemplate<String, Object> redisTemplate, SaTokenResponses tokenResponses) {
        this.redisTemplate = redisTemplate;
        this.tokenResponses = tokenResponses;
    }

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
        return tokenResponses.issue(normalized);
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
}
