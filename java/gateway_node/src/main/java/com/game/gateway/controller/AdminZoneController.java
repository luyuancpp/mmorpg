package com.game.gateway.controller;

import com.game.gateway.entity.ZoneConfig;
import com.game.gateway.repository.ZoneConfigRepository;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.util.List;

@RestController
@RequestMapping("/admin/zones")
public class AdminZoneController {

    private final ZoneConfigRepository zoneConfigRepo;

    public AdminZoneController(ZoneConfigRepository zoneConfigRepo) {
        this.zoneConfigRepo = zoneConfigRepo;
    }

    @GetMapping
    public List<ZoneConfig> listAll() {
        return zoneConfigRepo.findAll();
    }

    @GetMapping("/{zoneId}")
    public ResponseEntity<ZoneConfig> getById(@PathVariable Long zoneId) {
        return zoneConfigRepo.findById(zoneId)
                .map(ResponseEntity::ok)
                .orElse(ResponseEntity.notFound().build());
    }

    @PostMapping
    public ZoneConfig create(@RequestBody ZoneConfig zone) {
        return zoneConfigRepo.save(zone);
    }

    @PutMapping("/{zoneId}")
    public ResponseEntity<ZoneConfig> update(@PathVariable Long zoneId, @RequestBody ZoneConfig body) {
        return zoneConfigRepo.findById(zoneId).map(existing -> {
            existing.setName(body.getName());
            existing.setManualStatus(body.getManualStatus());
            existing.setCapacity(body.getCapacity());
            existing.setMaintenanceMsg(body.getMaintenanceMsg());
            existing.setOpenTime(body.getOpenTime());
            existing.setRecommended(body.isRecommended());
            existing.setSortOrder(body.getSortOrder());
            return ResponseEntity.ok(zoneConfigRepo.save(existing));
        }).orElse(ResponseEntity.notFound().build());
    }

    @DeleteMapping("/{zoneId}")
    public ResponseEntity<Void> delete(@PathVariable Long zoneId) {
        if (zoneConfigRepo.existsById(zoneId)) {
            zoneConfigRepo.deleteById(zoneId);
            return ResponseEntity.noContent().build();
        }
        return ResponseEntity.notFound().build();
    }

    /**
     * Quick action: set a zone to maintenance.
     * POST /admin/zones/1/maintenance
     * Body: { "maintenanceMsg": "Estimated recovery at 14:00" }
     */
    @PostMapping("/{zoneId}/maintenance")
    public ResponseEntity<ZoneConfig> setMaintenance(@PathVariable Long zoneId,
                                                     @RequestBody(required = false) MaintenanceRequest req) {
        return zoneConfigRepo.findById(zoneId).map(zone -> {
            zone.setManualStatus(1); // MAINTENANCE
            if (req != null && req.maintenanceMsg != null) {
                zone.setMaintenanceMsg(req.maintenanceMsg);
            }
            return ResponseEntity.ok(zoneConfigRepo.save(zone));
        }).orElse(ResponseEntity.notFound().build());
    }

    /**
     * Quick action: set a zone back to open.
     * POST /admin/zones/1/open
     */
    @PostMapping("/{zoneId}/open")
    public ResponseEntity<ZoneConfig> setOpen(@PathVariable Long zoneId) {
        return zoneConfigRepo.findById(zoneId).map(zone -> {
            zone.setManualStatus(0); // OPEN
            zone.setMaintenanceMsg("");
            return ResponseEntity.ok(zoneConfigRepo.save(zone));
        }).orElse(ResponseEntity.notFound().build());
    }

    public record MaintenanceRequest(String maintenanceMsg) {}
}
