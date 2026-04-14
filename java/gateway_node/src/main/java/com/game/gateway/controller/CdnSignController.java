package com.game.gateway.controller;

import com.game.gateway.dto.CdnSignRequest;
import com.game.gateway.dto.CdnSignResponse;
import com.game.gateway.service.CdnSignService;
import org.springframework.web.bind.annotation.*;

@RestController
@RequestMapping("/api")
public class CdnSignController {

    private final CdnSignService cdnSignService;

    public CdnSignController(CdnSignService cdnSignService) {
        this.cdnSignService = cdnSignService;
    }

    @PostMapping("/cdn-sign")
    public CdnSignResponse cdnSign(@RequestBody CdnSignRequest req) {
        return cdnSignService.sign(req);
    }
}
