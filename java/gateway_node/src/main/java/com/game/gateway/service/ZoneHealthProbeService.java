package com.game.gateway.service;

import com.game.gateway.config.ZoneProbeProperties;
import com.game.gateway.dto.AutoZoneStatus;
import com.game.gateway.dto.LoadLevel;
import com.game.gateway.entity.ZoneConfig;
import com.game.gateway.etcd.GateWatcher;
import com.game.gateway.etcd.NodeInfoRecord;
import com.game.gateway.repository.ZoneConfigRepository;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.scheduling.annotation.Scheduled;
import org.springframework.stereotype.Service;

import java.util.*;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Periodically probes etcd + Redis to determine each zone's auto health status and load level.
 * Results are cached in-memory for fast reads by the server-list API.
 */
@Service
public class ZoneHealthProbeService {

    private static final Logger log = LoggerFactory.getLogger(ZoneHealthProbeService.class);

    private final GateWatcher gateWatcher;
    private final ZoneConfigRepository zoneConfigRepo;
    private final StringRedisTemplate redisTemplate;
    private final ZoneProbeProperties probeProps;

    /** zone_id -> auto status */
    private final Map<Long, AutoZoneStatus> autoStatusCache = new ConcurrentHashMap<>();
    /** zone_id -> load level */
    private final Map<Long, LoadLevel> loadLevelCache = new ConcurrentHashMap<>();
    /** zone_id -> total online player count */
    private final Map<Long, Long> onlineCountCache = new ConcurrentHashMap<>();

    public ZoneHealthProbeService(GateWatcher gateWatcher,
                                  ZoneConfigRepository zoneConfigRepo,
                                  StringRedisTemplate redisTemplate,
                                  ZoneProbeProperties probeProps) {
        this.gateWatcher = gateWatcher;
        this.zoneConfigRepo = zoneConfigRepo;
        this.redisTemplate = redisTemplate;
        this.probeProps = probeProps;
    }

    @Scheduled(fixedDelayString = "${zone-probe.interval-ms:5000}")
    public void probe() {
        try {
            List<ZoneConfig> zones = zoneConfigRepo.findAll();
            List<NodeInfoRecord> gateNodes = gateWatcher.fetchAllGateNodes();
            List<NodeInfoRecord> sceneNodes = gateWatcher.fetchAllSceneNodes();

            // Group by zone
            Map<Long, List<NodeInfoRecord>> gatesByZone = groupByZone(gateNodes);
            Map<Long, List<NodeInfoRecord>> scenesByZone = groupByZone(sceneNodes);

            for (ZoneConfig zone : zones) {
                long zoneId = zone.getZoneId();
                List<NodeInfoRecord> gates = gatesByZone.getOrDefault(zoneId, Collections.emptyList());
                List<NodeInfoRecord> scenes = scenesByZone.getOrDefault(zoneId, Collections.emptyList());

                // Evaluate health
                AutoZoneStatus autoStatus = evaluateHealth(zoneId, gates, scenes);
                autoStatusCache.put(zoneId, autoStatus);

                // Calculate load
                long totalPlayers = gates.stream().mapToLong(NodeInfoRecord::getPlayerCount).sum();
                onlineCountCache.put(zoneId, totalPlayers);
                loadLevelCache.put(zoneId, calculateLoadLevel(totalPlayers, zone.getCapacity()));
            }

            log.debug("Zone probe completed: {} zones checked", zones.size());
        } catch (Exception e) {
            log.error("Zone health probe failed: {}", e.getMessage(), e);
        }
    }

    private AutoZoneStatus evaluateHealth(long zoneId,
                                          List<NodeInfoRecord> gates,
                                          List<NodeInfoRecord> scenes) {
        boolean hasGate = !gates.isEmpty();
        boolean hasScene = !scenes.isEmpty();

        if (hasGate && hasScene) {
            return AutoZoneStatus.HEALTHY;
        } else if (hasGate || hasScene) {
            return AutoZoneStatus.DEGRADED;
        } else {
            return AutoZoneStatus.DOWN;
        }
    }

    private LoadLevel calculateLoadLevel(long totalPlayers, int capacity) {
        if (capacity <= 0) return LoadLevel.SMOOTH;
        double ratio = (double) totalPlayers / capacity;
        if (ratio < 0.5) return LoadLevel.SMOOTH;
        if (ratio < 0.8) return LoadLevel.BUSY;
        return LoadLevel.FULL;
    }

    private Map<Long, List<NodeInfoRecord>> groupByZone(List<NodeInfoRecord> nodes) {
        Map<Long, List<NodeInfoRecord>> map = new HashMap<>();
        for (NodeInfoRecord n : nodes) {
            map.computeIfAbsent(n.getZoneId(), k -> new ArrayList<>()).add(n);
        }
        return map;
    }

    public AutoZoneStatus getAutoStatus(long zoneId) {
        return autoStatusCache.getOrDefault(zoneId, AutoZoneStatus.DOWN);
    }

    public LoadLevel getLoadLevel(long zoneId) {
        return loadLevelCache.getOrDefault(zoneId, LoadLevel.SMOOTH);
    }

    public long getOnlineCount(long zoneId) {
        return onlineCountCache.getOrDefault(zoneId, 0L);
    }
}
