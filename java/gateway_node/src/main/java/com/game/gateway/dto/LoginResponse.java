package com.game.gateway.dto;

import com.fasterxml.jackson.annotation.JsonInclude;

import java.util.List;

/**
 * Response for {@code POST /api/login}.
 *
 * <p>Three terminal shapes:
 * <ul>
 *   <li>code = 0   — success: tokens & player list populated.</li>
 *   <li>code = 100 — QUEUEING: client should retry after {@code retryAfterMs}.</li>
 *   <li>code = 101 — QUEUE_TIMEOUT: hard stop, advise the player.</li>
 *   <li>code = 401 — auth rejected (e.g. WeChat code expired).</li>
 *   <li>code = 429 — rate-limited (IP cap / account cooldown).</li>
 *   <li>code = 500 — internal error.</li>
 * </ul>
 *
 * <p>{@code retryAfterMs} / {@code queuePos} are only meaningful when code=100.
 */
@JsonInclude(JsonInclude.Include.NON_NULL)
public class LoginResponse {

    public static final int CODE_OK             = 0;
    public static final int CODE_QUEUEING       = 100;
    public static final int CODE_QUEUE_TIMEOUT  = 101;
    public static final int CODE_AUTH_REJECTED  = 401;
    public static final int CODE_RATE_LIMITED   = 429;
    public static final int CODE_INTERNAL       = 500;

    private int code;
    private String message;

    // queue payload
    private Long retryAfterMs;
    private Long queuePos;

    // success payload
    private List<PlayerInfo> players;
    private String accessToken;
    private String refreshToken;
    private Long accessTokenExpire;
    private Long refreshTokenExpire;

    public static LoginResponse ok(List<PlayerInfo> players,
                                   String accessToken, String refreshToken,
                                   long accessExpire, long refreshExpire) {
        LoginResponse r = new LoginResponse();
        r.code = CODE_OK;
        r.players = players;
        r.accessToken = accessToken;
        r.refreshToken = refreshToken;
        r.accessTokenExpire = accessExpire;
        r.refreshTokenExpire = refreshExpire;
        return r;
    }

    public static LoginResponse queueing(long retryAfterMs, long queuePos) {
        LoginResponse r = new LoginResponse();
        r.code = CODE_QUEUEING;
        r.message = "QUEUEING";
        r.retryAfterMs = retryAfterMs;
        r.queuePos = queuePos;
        return r;
    }

    public static LoginResponse error(int code, String message) {
        LoginResponse r = new LoginResponse();
        r.code = code;
        r.message = message;
        return r;
    }

    public static class PlayerInfo {
        private long playerId;
        private String name;
        private int level;

        public long getPlayerId() { return playerId; }
        public void setPlayerId(long playerId) { this.playerId = playerId; }
        public String getName() { return name; }
        public void setName(String name) { this.name = name; }
        public int getLevel() { return level; }
        public void setLevel(int level) { this.level = level; }
    }

    public int getCode() { return code; }
    public void setCode(int code) { this.code = code; }
    public String getMessage() { return message; }
    public void setMessage(String message) { this.message = message; }
    public Long getRetryAfterMs() { return retryAfterMs; }
    public void setRetryAfterMs(Long retryAfterMs) { this.retryAfterMs = retryAfterMs; }
    public Long getQueuePos() { return queuePos; }
    public void setQueuePos(Long queuePos) { this.queuePos = queuePos; }
    public List<PlayerInfo> getPlayers() { return players; }
    public void setPlayers(List<PlayerInfo> players) { this.players = players; }
    public String getAccessToken() { return accessToken; }
    public void setAccessToken(String accessToken) { this.accessToken = accessToken; }
    public String getRefreshToken() { return refreshToken; }
    public void setRefreshToken(String refreshToken) { this.refreshToken = refreshToken; }
    public Long getAccessTokenExpire() { return accessTokenExpire; }
    public void setAccessTokenExpire(Long accessTokenExpire) { this.accessTokenExpire = accessTokenExpire; }
    public Long getRefreshTokenExpire() { return refreshTokenExpire; }
    public void setRefreshTokenExpire(Long refreshTokenExpire) { this.refreshTokenExpire = refreshTokenExpire; }
}
