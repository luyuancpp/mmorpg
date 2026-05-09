package com.game.gateway.controller;

import com.game.gateway.dto.RefreshTokenRequest;
import com.game.gateway.dto.RefreshTokenResponse;
import com.game.gateway.service.RefreshTokenService;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

/**
 * Dedicated HTTP endpoint for refresh-token rotation.
 *
 * <p>Refresh-token rotation does not need session affinity or a TCP
 * connection to the gate — it's a stateless Redis lookup in go-zero login.
 * Keeping it on HTTP moves the rotation path out of the game TCP channel so
 * gate stays focused on connected-player traffic.
 *
 * <p>This controller is deliberately <i>not</i> rate-limited the same way
 * {@code /api/login} is: refresh is already cheap (two Redis ops) and the
 * per-account cooldown in {@link com.game.gateway.ratelimit.AssignGateRateLimiter}
 * would prevent legitimate in-game token renewal. IP/global protection, if
 * needed, can be added later with a separate properties section.
 */
@RestController
@RequestMapping("/api")
public class RefreshTokenController {

    private final RefreshTokenService service;

    public RefreshTokenController(RefreshTokenService service) {
        this.service = service;
    }

    @PostMapping("/refresh-token")
    public RefreshTokenResponse refresh(@RequestBody RefreshTokenRequest req) {
        return service.refresh(req);
    }
}
