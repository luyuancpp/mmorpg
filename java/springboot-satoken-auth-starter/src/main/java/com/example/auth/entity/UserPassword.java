package com.example.auth.entity;

import jakarta.persistence.*;
import lombok.Data;

@Entity
@Table(name = "user_password")
@Data
public class UserPassword {
    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column(name = "user_id")
    private Long userId;

    public String getPasswordHash() {
        return passwordHash;
    }

    @Column(name = "password_hash")
    private String passwordHash;
}
