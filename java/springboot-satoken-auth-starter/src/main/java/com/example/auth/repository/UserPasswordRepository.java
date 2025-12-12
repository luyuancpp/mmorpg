package com.example.auth.repository;

import com.example.auth.entity.UserPassword;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.stereotype.Repository;

import java.util.Optional;

@Repository
public interface UserPasswordRepository extends JpaRepository<UserPassword, Long> {
    Optional<UserPassword> findByUserId(Long userId);
}
