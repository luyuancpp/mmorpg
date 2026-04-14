package com.game.gateway.controller;

import com.game.gateway.entity.ZoneWhitelist;
import com.game.gateway.repository.ZoneWhitelistRepository;
import org.springframework.http.ResponseEntity;
import org.springframework.transaction.annotation.Transactional;
import org.springframework.web.bind.annotation.*;

import java.util.List;

@RestController
@RequestMapping("/admin/whitelist")
public class AdminWhitelistController {

    private final ZoneWhitelistRepository whitelistRepo;

    public AdminWhitelistController(ZoneWhitelistRepository whitelistRepo) {
        this.whitelistRepo = whitelistRepo;
    }

    @GetMapping("/{zoneId}")
    public List<ZoneWhitelist> listByZone(@PathVariable Long zoneId) {
        return whitelistRepo.findByZoneId(zoneId);
    }

    @PostMapping
    public ZoneWhitelist add(@RequestBody ZoneWhitelist entry) {
        return whitelistRepo.save(entry);
    }

    @DeleteMapping("/{zoneId}/{accountId}")
    @Transactional
    public ResponseEntity<Void> remove(@PathVariable Long zoneId, @PathVariable Long accountId) {
        whitelistRepo.deleteByZoneIdAndAccountId(zoneId, accountId);
        return ResponseEntity.noContent().build();
    }
}
