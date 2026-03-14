package com.example.auth.service;

import cn.dev33.satoken.stp.StpUtil;
import com.example.auth.entity.User;
import com.example.auth.entity.UserPassword;
import com.example.auth.entity.UserOauth;
import com.example.auth.repository.UserOauthRepository;
import com.example.auth.repository.UserPasswordRepository;
import com.example.auth.repository.UserRepository;
import org.springframework.security.crypto.bcrypt.BCryptPasswordEncoder;
import org.springframework.stereotype.Service;

import java.time.LocalDateTime;
import java.util.Optional;

@Service
public class UserService {
    private final UserRepository userRepository;
    private final UserPasswordRepository passwordRepository;
    private final UserOauthRepository oauthRepository;
    private final BCryptPasswordEncoder encoder = new BCryptPasswordEncoder();

    public UserService(UserRepository userRepository, UserPasswordRepository passwordRepository, UserOauthRepository oauthRepository) {
        this.userRepository = userRepository;
        this.passwordRepository = passwordRepository;
        this.oauthRepository = oauthRepository;
    }

    public Optional<User> findById(Long id) {
        return userRepository.findById(id);
    }

    public User createGuest() {
        User u = new User();
        u.setId(System.currentTimeMillis());
        u.setCreateTime(LocalDateTime.now());
        u.setLastLogin(LocalDateTime.now());
        u.setIsGuest(true);
        u.setNickname("guest_" + (u.getId() % 10000));
        return userRepository.save(u);
    }

    public User getOrCreateByPhone(String phone) {
        User u = new User();
        u.setId(System.currentTimeMillis());
        u.setCreateTime(LocalDateTime.now());
        u.setLastLogin(LocalDateTime.now());
        u.setIsGuest(false);
        u.setNickname("user_" + (u.getId() % 10000));
        User saved = userRepository.save(u);
        return saved;
    }

    public boolean verifyPassword(Long userId, String rawPwd) {
        Optional<UserPassword> up = passwordRepository.findByUserId(userId);
        return up.isPresent() && encoder.matches(rawPwd, up.get().getPasswordHash());
    }

    public void bindOauthToUser(String provider, String openid, Long userId) {
        UserOauth uo = new UserOauth();
        uo.setProvider(provider);
        uo.setOpenId(openid);
        uo.setUserId(userId);
        oauthRepository.save(uo);
    }

    public Optional<UserOauth> findOauth(String provider, String openid) {
        return oauthRepository.findByProviderAndOpenId(provider, openid);
    }
}
