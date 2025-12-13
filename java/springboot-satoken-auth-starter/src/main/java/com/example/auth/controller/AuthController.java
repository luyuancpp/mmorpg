package com.example.auth.controller;

import cn.dev33.satoken.stp.StpUtil;
import me.zhyd.oauth.config.AuthConfig;
import me.zhyd.oauth.enums.AuthResponseStatus;
import me.zhyd.oauth.model.AuthCallback;
import me.zhyd.oauth.model.AuthUser;
import me.zhyd.oauth.request.AuthRequest;
import me.zhyd.oauth.utils.AuthStateUtils;
import org.springframework.web.bind.annotation.*;

import me.zhyd.oauth.request.AuthGithubRequest;

@RestController
@RequestMapping("/auth")
public class AuthController {

    // 跳转到授权页
    @GetMapping("/login/{provider}")
    public String renderAuth(@PathVariable("provider") String provider) {
        AuthRequest authRequest = getAuthRequest(provider);
        return authRequest.authorize(AuthStateUtils.createState());
    }

    @GetMapping("/callback/{provider}")
    public Object login(@PathVariable String provider, AuthCallback callback) {
        AuthRequest authRequest = getAuthRequest(provider);
        var response = authRequest.login(callback);

        if (response.getCode() == AuthResponseStatus.SUCCESS.getCode()) {

            // ⬇⬇⬇ 关键：强转为 AuthUser
            AuthUser authUser = (AuthUser) response.getData();

            // JustAuth 规范里的三方唯一 ID
            String uuid = authUser.getUuid();

            // 登录 Sa-Token（临时用 uuid，后面你会换成 userId）
            StpUtil.login(uuid);

            return StpUtil.getTokenInfo();
        }
        return response;
    }


    // 根据 provider 创建 AuthRequest（新版 JustAuth）
    private AuthRequest getAuthRequest(String provider) {
        switch (provider) {
            case "github":
                return new AuthGithubRequest(AuthConfig.builder()
                        .clientId("你的ClientId")
                        .clientSecret("你的ClientSecret")
                        .redirectUri("http://localhost:8080/auth/callback/github")
                        .build());
            default:
                throw new RuntimeException("不支持的 provider: " + provider);
        }
    }
}
