package com.example.auth.service;

import org.springframework.data.redis.core.RedisTemplate;
import org.springframework.stereotype.Service;

import java.time.Duration;

@Service
public class SmsService {

    private final RedisTemplate<String, Object> redisTemplate;

    public SmsService(RedisTemplate<String, Object> redisTemplate) {
        this.redisTemplate = redisTemplate;
    }

    public void sendCode(String phone) {
        String code = String.valueOf((int) ((Math.random() * 9 + 1) * 100000));
        redisTemplate.opsForValue().set("sms:code:" + phone, code, Duration.ofMinutes(5));
        System.out.println("[SMS] send to " + phone + " code=" + code);
    }

    public boolean verify(String phone, String code) {
        Object v = redisTemplate.opsForValue().get("sms:code:" + phone);
        return v != null && v.toString().equals(code);
    }
}
