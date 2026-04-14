package com.game.gateway.service;

import com.game.gateway.entity.Announcement;
import com.game.gateway.repository.AnnouncementRepository;
import org.springframework.stereotype.Service;

import java.time.LocalDateTime;
import java.util.List;

@Service
public class AnnouncementService {

    private final AnnouncementRepository announcementRepo;

    public AnnouncementService(AnnouncementRepository announcementRepo) {
        this.announcementRepo = announcementRepo;
    }

    public List<Announcement> getActiveAnnouncements() {
        return announcementRepo.findActive(LocalDateTime.now());
    }

    public List<Announcement> getAll() {
        return announcementRepo.findAll();
    }

    public Announcement create(Announcement announcement) {
        return announcementRepo.save(announcement);
    }

    public void delete(Long id) {
        announcementRepo.deleteById(id);
    }
}
