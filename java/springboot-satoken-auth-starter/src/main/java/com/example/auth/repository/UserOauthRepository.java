package com.example.auth.repository;

import com.example.auth.entity.UserOauth;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.stereotype.Repository;

import java.util.Optional;

@Repository
public interface UserOauthRepository extends JpaRepository<UserOauth, Long> {
    Optional<UserOauth> findByProviderAndOpenid(String provider, String openid);
}
