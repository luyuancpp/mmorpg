package com.game.gateway.dto;

/** Inbound payload for {@code POST /api/refresh-token}. */
public class RefreshTokenRequest {

    /** Current refresh token; will be rotated (one-time-use) on success. */
    private String refreshToken;

    public String getRefreshToken() { return refreshToken; }
    public void setRefreshToken(String refreshToken) { this.refreshToken = refreshToken; }
}
