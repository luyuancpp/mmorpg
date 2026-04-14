package com.game.gateway.dto;

public class CdnSignResponse {
    private String signedUrl;
    private long expireAt;

    public String getSignedUrl() { return signedUrl; }
    public void setSignedUrl(String signedUrl) { this.signedUrl = signedUrl; }

    public long getExpireAt() { return expireAt; }
    public void setExpireAt(long expireAt) { this.expireAt = expireAt; }
}
