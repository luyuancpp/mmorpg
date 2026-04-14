package com.game.gateway.controller;

import com.game.gateway.dto.AssignGateRequest;
import com.game.gateway.dto.AssignGateResponse;
import com.game.gateway.service.AssignGateService;
import org.springframework.web.bind.annotation.*;

@RestController
@RequestMapping("/api")
public class AssignGateController {

    private final AssignGateService assignGateService;

    public AssignGateController(AssignGateService assignGateService) {
        this.assignGateService = assignGateService;
    }

    @PostMapping("/assign-gate")
    public AssignGateResponse assignGate(@RequestBody AssignGateRequest req) {
        return assignGateService.assignGate(req);
    }
}
