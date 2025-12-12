package com.example.auth.entity;

import jakarta.persistence.*;
import lombok.Data;
import java.time.LocalDateTime;

@Entity
@Table(name = "user")
@Data
public class User {
    @Id
    private Long id;

    @Column(name = "create_time")
    private LocalDateTime createTime;

    @Column(name = "last_login")
    private LocalDateTime lastLogin;

    @Column(name = "is_guest")
    private Boolean isGuest;

    private String avatar;
    private String nickname;
}
