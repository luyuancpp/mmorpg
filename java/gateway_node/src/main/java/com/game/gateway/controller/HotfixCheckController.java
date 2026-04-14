package com.game.gateway.controller;

import com.game.gateway.dto.HotfixCheckRequest;
import com.game.gateway.dto.HotfixCheckResponse;
import com.game.gateway.service.HotfixCheckService;
import org.springframework.web.bind.annotation.*;

@RestController
@RequestMapping("/api")
public class HotfixCheckController {

    private final HotfixCheckService hotfixCheckService;

    public HotfixCheckController(HotfixCheckService hotfixCheckService) {
        this.hotfixCheckService = hotfixCheckService;
    }

    @PostMapping("/hotfix-check")
    public HotfixCheckResponse hotfixCheck(@RequestBody HotfixCheckRequest req) {
        return hotfixCheckService.check(req);
    }
}
