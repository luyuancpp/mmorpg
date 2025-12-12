package com.example.auth.dto;

import lombok.Data;

@Data
public class OauthCallbackDTO {
    private String code;
    private String state;
}
