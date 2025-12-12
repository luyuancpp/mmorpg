package com.example.auth.service;

import com.example.auth.entity.User;
import com.example.auth.entity.UserOauth;
import com.example.auth.repository.UserOauthRepository;
import com.example.auth.repository.UserRepository;
import org.springframework.stereotype.Service;

import java.time.LocalDateTime;
import java.util.Optional;

@Service
public class OauthService {

    private final UserOauthRepository oauthRepository;
    private final UserRepository userRepository;

    public OauthService(UserOauthRepository oauthRepository, UserRepository userRepository) {
        this.oauthRepository = oauthRepository;
        this.userRepository = userRepository;
    }

    /**
     * 找到绑定用户或创建一个新用户并写入 user_oauth
     */
    public Long findOrCreateAndBind(String provider, String openid, String nickname, String avatar) {
        Optional<UserOauth> maybe = oauthRepository.findByProviderAndOpenid(provider, openid);
        if (maybe.isPresent()) {
            return maybe.get().getUserId();
        }
        // create user
        User u = new User();
        u.setId(System.currentTimeMillis());
        u.setCreateTime(LocalDateTime.now());
        u.setLastLogin(LocalDateTime.now());
        u.setIsGuest(false);
        u.setNickname(nickname == null ? provider + "_" + (u.getId() % 10000) : nickname);
        u.setAvatar(avatar);
        User saved = userRepository.save(u);

        // bind
        UserOauth uo = new UserOauth();
        uo.setProvider(provider);
        uo.setOpenid(openid);
        uo.setUserId(saved.getId());
        oauthRepository.save(uo);

        return saved.getId();
    }
}
