package com.game.gateway.repository;

import com.game.gateway.entity.Announcement;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;

import java.time.LocalDateTime;
import java.util.List;

public interface AnnouncementRepository extends JpaRepository<Announcement, Long> {

    @Query("SELECT a FROM Announcement a WHERE " +
           "(a.startTime IS NULL OR a.startTime <= :now) AND " +
           "(a.endTime IS NULL OR a.endTime >= :now) " +
           "ORDER BY a.createdAt DESC")
    List<Announcement> findActive(LocalDateTime now);
}
