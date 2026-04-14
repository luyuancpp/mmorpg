package com.game.gateway.controller;

import com.game.gateway.entity.Announcement;
import com.game.gateway.service.AnnouncementService;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

import java.time.ZoneOffset;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

@RestController
@RequestMapping("/api")
public class AnnouncementController {

    private final AnnouncementService announcementService;

    public AnnouncementController(AnnouncementService announcementService) {
        this.announcementService = announcementService;
    }

    @GetMapping("/announcement")
    public Map<String, Object> getAnnouncements() {
        List<Announcement> active = announcementService.getActiveAnnouncements();
        List<Map<String, Object>> items = active.stream().map(a -> {
            Map<String, Object> m = new HashMap<>();
            m.put("id", a.getId());
            m.put("title", a.getTitle());
            m.put("content", a.getContent());
            m.put("type", a.getType());
            if (a.getStartTime() != null) {
                m.put("start_time", a.getStartTime().toEpochSecond(ZoneOffset.UTC));
            }
            if (a.getEndTime() != null) {
                m.put("end_time", a.getEndTime().toEpochSecond(ZoneOffset.UTC));
            }
            return m;
        }).toList();

        Map<String, Object> resp = new HashMap<>();
        resp.put("items", items);
        return resp;
    }
}
