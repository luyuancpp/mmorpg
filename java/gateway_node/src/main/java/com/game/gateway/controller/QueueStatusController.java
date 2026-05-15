package com.game.gateway.controller;

import com.game.gateway.dto.AssignGateResponse;
import com.game.gateway.dto.QueueStatusRequest;
import com.game.gateway.service.AssignGateService;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

/**
 * Polling endpoint for the AssignGate login queue. Clients call this every
 * {@code retryAfterMs} until the response carries {@code code=0} (admitted,
 * gate token populated) or {@code code=410} (token expired — restart from
 * /api/assign-gate).
 *
 * <p>Response shape mirrors {@link AssignGateResponse} so clients can reuse
 * the same parsing path as the initial /assign-gate response. The duplication
 * is intentional: a client that received {@code queueSource="login"} from
 * /assign-gate switches to /queue-status without changing its expected shape.
 *
 * <p>This endpoint deliberately bypasses {@code AssignGateRateLimiter}: the
 * caller is already in the queue (already throttled by definition), and the
 * only thing rate-limiting their polls would do is delay rank updates,
 * which is a UX regression with no security upside. Bucket4j stays in front
 * of /assign-gate where it actually filters new entrants.
 */
@RestController
@RequestMapping("/api")
public class QueueStatusController {

    private final AssignGateService assignGateService;

    public QueueStatusController(AssignGateService assignGateService) {
        this.assignGateService = assignGateService;
    }

    @PostMapping("/queue-status")
    public AssignGateResponse queueStatus(@RequestBody QueueStatusRequest req) {
        return assignGateService.queryQueueStatus(req.getQueueToken());
    }
}
