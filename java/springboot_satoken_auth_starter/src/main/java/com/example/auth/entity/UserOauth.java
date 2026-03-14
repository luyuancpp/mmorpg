package com.example.auth.entity;

import jakarta.persistence.*;

@Entity
@Table(
        name = "user_oauth",
        uniqueConstraints = {
                @UniqueConstraint(columnNames = {"provider", "openId"})
        }
)
public class UserOauth {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    private Long userId;

    private String provider;

    public void setId(Long id) {
        this.id = id;
    }

    private String openId;

    // ===== getter / setter =====

    public Long getUserId() {
        return userId;
    }

    public void setUserId(Long userId) {
        this.userId = userId;
    }

    public String getProvider() {
        return provider;
    }

    public void setProvider(String provider) {
        this.provider = provider;
    }

    public String getOpenId() {
        return openId;
    }

    public void setOpenId(String openId) {
        this.openId = openId;
    }
}
