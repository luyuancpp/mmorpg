package com.game.gateway.controller;

import com.game.gateway.dto.AssignGateRequest;
import com.game.gateway.dto.AssignGateResponse;
import com.game.gateway.ratelimit.AssignGateRateLimiter;
import com.game.gateway.ratelimit.RateLimitDecision;
import com.game.gateway.service.AssignGateService;
import jakarta.servlet.http.HttpServletRequest;
import org.springframework.web.bind.annotation.*;

@RestController
@RequestMapping("/api")
public class AssignGateController {

    private final AssignGateService assignGateService;
    private final AssignGateRateLimiter limiter;

    public AssignGateController(AssignGateService assignGateService, AssignGateRateLimiter limiter) {
        this.assignGateService = assignGateService;
        this.limiter = limiter;
    }

    @PostMapping("/assign-gate")
    public AssignGateResponse assignGate(@RequestBody AssignGateRequest req, HttpServletRequest http) {
        String ip = LoginController.extractIp(http);
        RateLimitDecision decision = limiter.check(req.getZoneId(), ip, req.getAccount());
        if (decision.isQueue()) {
            return AssignGateResponse.queueing(decision.getRetryAfterMs(), decision.getQueuePosEstimate());
        }
        if (decision.isDeny()) {
            return AssignGateResponse.rateLimited(decision.getReason());
        }
        return assignGateService.assignGate(req);
    }
}
