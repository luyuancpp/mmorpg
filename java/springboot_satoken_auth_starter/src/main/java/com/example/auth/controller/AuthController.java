package com.example.auth.controller;

import cn.dev33.satoken.stp.StpUtil;
import me.zhyd.oauth.config.AuthConfig;
import me.zhyd.oauth.enums.AuthResponseStatus;
import me.zhyd.oauth.model.AuthCallback;
import me.zhyd.oauth.model.AuthResponse;
import me.zhyd.oauth.model.AuthUser;
import me.zhyd.oauth.request.AuthRequest;
import me.zhyd.oauth.utils.AuthStateUtils;
import org.springframework.web.bind.annotation.*;

import me.zhyd.oauth.request.AuthGithubRequest;

@RestController
@RequestMapping("/auth")
public class AuthController {

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

            return StpUtil.getTokenInfo();
        }
        return response;
    }


    // Create AuthRequest for the given provider (JustAuth)
    private AuthRequest getAuthRequest(String provider) {
        switch (provider) {
            case "github":
                return new AuthGithubRequest(AuthConfig.builder()
                        .clientId("YOUR_CLIENT_ID")
                        .clientSecret("YOUR_CLIENT_SECRET")
                        .redirectUri("http://localhost:8080/auth/callback/github")
                        .build());
            default:
                throw new RuntimeException("Unsupported provider: " + provider);
        }
    }
}
