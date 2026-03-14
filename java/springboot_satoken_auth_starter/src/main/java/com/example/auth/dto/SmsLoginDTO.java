package com.example.auth.dto;

import lombok.Data;

@Data
public class SmsLoginDTO {
    private String phone;
    private String code;
}
