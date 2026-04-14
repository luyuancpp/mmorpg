package com.game.gateway.controller;

import com.game.gateway.entity.Announcement;
import com.game.gateway.service.AnnouncementService;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.util.List;

@RestController
@RequestMapping("/admin/announcements")
public class AdminAnnouncementController {

    private final AnnouncementService announcementService;

    public AdminAnnouncementController(AnnouncementService announcementService) {
        this.announcementService = announcementService;
    }

    @GetMapping
    public List<Announcement> listAll() {
        return announcementService.getAll();
    }

    @PostMapping
    public Announcement create(@RequestBody Announcement announcement) {
        return announcementService.create(announcement);
    }

    @DeleteMapping("/{id}")
    public ResponseEntity<Void> delete(@PathVariable Long id) {
        announcementService.delete(id);
        return ResponseEntity.noContent().build();
    }
}
