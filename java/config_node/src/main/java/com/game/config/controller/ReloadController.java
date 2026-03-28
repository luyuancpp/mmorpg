package com.game.config.controller;

import com.game.config.service.TableService;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.util.Map;

/**
 * Hot-reload endpoint for config tables.
 * POST /api/reload triggers a full table reload.
 */
@RestController
@RequestMapping("/api/reload")
public class ReloadController {

    private final TableService tableService;

    public ReloadController(TableService tableService) {
        this.tableService = tableService;
    }

    @PostMapping
    public ResponseEntity<Map<String, Object>> reload() {
        tableService.reload();
        return ResponseEntity.ok(Map.of(
            "status", "ok",
            "loadVersion", tableService.getLoadVersion(),
            "lastLoadTimeMs", tableService.getLastLoadTimeMs()
        ));
    }
}
