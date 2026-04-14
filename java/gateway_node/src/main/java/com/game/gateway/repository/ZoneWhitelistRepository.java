package com.game.gateway.repository;

import com.game.gateway.entity.ZoneWhitelist;
import org.springframework.data.jpa.repository.JpaRepository;

import java.util.List;

public interface ZoneWhitelistRepository extends JpaRepository<ZoneWhitelist, Long> {
    List<ZoneWhitelist> findByZoneId(Long zoneId);
    boolean existsByZoneIdAndAccountId(Long zoneId, Long accountId);
    void deleteByZoneIdAndAccountId(Long zoneId, Long accountId);
}
