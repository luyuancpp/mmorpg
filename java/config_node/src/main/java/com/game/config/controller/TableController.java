package com.game.config.controller;

import com.game.config.service.TableService;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.util.Map;

/**
 * REST endpoints for querying config tables.
 */
@RestController
@RequestMapping("/api/table")
public class TableController {

    private final TableService tableService;

    public TableController(TableService tableService) {
        this.tableService = tableService;
    }

    /**
     * GET /api/table/{tableName}/{id}
     * Query a single row by primary ID.
     */
    @GetMapping("/{tableName}/{id}")
    public ResponseEntity<String> getById(@PathVariable String tableName, @PathVariable int id) {
        String json = tableService.queryById(tableName, id);
        if (json == null) {
            return ResponseEntity.notFound().build();
        }
        return ResponseEntity.ok().header("Content-Type", "application/json").body(json);
    }

    /**
     * GET /api/table/status
     * Returns load version and timing info.
     */
    @GetMapping("/status")
    public Map<String, Object> status() {
        return Map.of(
            "loadVersion", tableService.getLoadVersion(),
            "lastLoadTimeMs", tableService.getLastLoadTimeMs()
        );
    }
}
