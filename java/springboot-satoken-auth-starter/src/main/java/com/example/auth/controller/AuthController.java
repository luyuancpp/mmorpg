package com.example.auth.controller;

import cn.dev33.satoken.stp.StpUtil;
import com.example.auth.dto.LoginDTO;
import com.example.auth.dto.OauthCallbackDTO;
import com.example.auth.dto.SmsLoginDTO;
import com.example.auth.entity.User;
import com.example.auth.repository.UserPasswordRepository;
import com.example.auth.repository.UserRepository;
import com.example.auth.service.OauthService;
import com.example.auth.service.SmsService;
import com.example.auth.service.UserService;
import me.zhyd.oauth.model.AuthUser;
import me.zhyd.oauth.request.AuthRequest;
import me.zhyd.oauth.request.AuthRequestFactory;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.util.HashMap;
import java.util.Map;
import java.util.Optional;

@RestController
@RequestMapping("/api/auth")
public class AuthController {

    private final UserService userService;
    private final SmsService smsService;
    private final UserPasswordRepository passwordRepository;
    private final OauthService oauthService;
    private final UserRepository userRepository;

    public AuthController(UserService userService, SmsService smsService, UserPasswordRepository passwordRepository, OauthService oauthService, UserRepository userRepository) {
        this.userService = userService;
        this.smsService = smsService;
        this.passwordRepository = passwordRepository;
        this.oauthService = oauthService;
        this.userRepository = userRepository;
    }

    @PostMapping("/login/password")
    public ResponseEntity<?> loginByPassword(@RequestBody LoginDTO dto) {
        Long demoId = 1L;
        if (!userService.verifyPassword(demoId, dto.getPassword())) {
            return ResponseEntity.badRequest().body("password incorrect");
        }
        StpUtil.login(demoId);
        return ResponseEntity.ok(Map.of("tokenInfo", StpUtil.getTokenInfo()));
    }

    @GetMapping("/login/sendSms")
    public ResponseEntity<?> sendSms(@RequestParam String phone) {
        smsService.sendCode(phone);
        return ResponseEntity.ok("sent");
    }

    @PostMapping("/login/sms")
    public ResponseEntity<?> loginBySms(@RequestBody SmsLoginDTO dto) {
        if (!smsService.verify(dto.getPhone(), dto.getCode())) {
            return ResponseEntity.badRequest().body("code invalid");
        }
        User u = userService.getOrCreateByPhone(dto.getPhone());
        StpUtil.login(u.getId());
        return ResponseEntity.ok(Map.of("tokenInfo", StpUtil.getTokenInfo()));
    }

    @PostMapping("/login/guest")
    public ResponseEntity<?> guest() {
        User u = userService.createGuest();
        StpUtil.login(u.getId());
        return ResponseEntity.ok(Map.of("tokenInfo", StpUtil.getTokenInfo()));
    }

    @GetMapping("/me")
    public ResponseEntity<?> me() {
        if (!StpUtil.isLogin()) return ResponseEntity.status(401).body("not login");
        long id = StpUtil.getLoginIdAsLong();
        return ResponseEntity.ok(Map.of("id", id));
    }

    // ---------------- JustAuth 示例（已改为保存用户并 bind） ----------------
    @GetMapping("/oauth/{provider}/redirect")
    public ResponseEntity<?> redirect(@PathVariable String provider) {
        try {
            AuthRequest authRequest = AuthRequestFactory.get(provider.toUpperCase());
            String url = authRequest.authorize();
            return ResponseEntity.ok(Map.of("url", url));
        } catch (Exception e) {
            return ResponseEntity.badRequest().body("provider not supported or not configured");
        }
    }

    @GetMapping("/oauth/{provider}/callback")
    public ResponseEntity<?> callback(@PathVariable String provider, OauthCallbackDTO callbackDTO) {
        try {
            AuthRequest authRequest = AuthRequestFactory.get(provider.toUpperCase());
            var authResponse = authRequest.login(callbackDTO);
            AuthUser authUser = (AuthUser) authResponse.getData();
            if (authUser == null) return ResponseEntity.badRequest().body("oauth failed");

            // 使用 OauthService 完整创建并绑定用户
            Long userId = oauthService.findOrCreateAndBind(provider, authUser.getUuid(), authUser.getNickname(), authUser.getAvatar());

            StpUtil.login(userId);
            Map<String, Object> r = new HashMap<>();
            r.put("tokenInfo", StpUtil.getTokenInfo());
            r.put("oauthUser", authUser);
            return ResponseEntity.ok(r);
        } catch (Exception e) {
            e.printStackTrace();
            return ResponseEntity.badRequest().body("callback error: " + e.getMessage());
        }
    }
}
