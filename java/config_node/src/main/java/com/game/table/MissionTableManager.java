
package com.game.table;

import com.google.protobuf.util.JsonFormat;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Auto-generated config manager for Mission.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public class MissionTableManager {

    private static final MissionTableManager INSTANCE = new MissionTableManager();

    private MissionTableData data;
    private final Map<Integer, MissionTable> kvData = new HashMap<>();


    private final Map<Integer, List<MissionTable>> idxCondition_id = new HashMap<>();

    private final Map<Integer, List<MissionTable>> idxNext_mission_id = new HashMap<>();

    private final Map<Integer, List<MissionTable>> idxTarget_count = new HashMap<>();


    public static MissionTableManager getInstance() {
        return INSTANCE;
    }

    public void load(String configDir, boolean useBinary) throws Exception {
        MissionTableData.Builder builder = MissionTableData.newBuilder();
        if (useBinary) {
            byte[] raw = Files.readAllBytes(Path.of(configDir, "mission.pb"));
            builder.mergeFrom(raw);
        } else {
            String json = Files.readString(Path.of(configDir, "mission.json"));
            JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
        }
        this.data = builder.build();

        for (MissionTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
            for (Integer elem : row.getConditionIdList()) {
                idxCondition_id.computeIfAbsent(elem, k -> new ArrayList<>()).add(row);
            }
            for (Integer elem : row.getNextMissionIdList()) {
                idxNext_mission_id.computeIfAbsent(elem, k -> new ArrayList<>()).add(row);
            }
            for (Integer elem : row.getTargetCountList()) {
                idxTarget_count.computeIfAbsent(elem, k -> new ArrayList<>()).add(row);
            }
        }
    }

    /** SELECT * FROM mission */
    public MissionTableData selectAll() {
        return data;
    }

    /** SELECT COUNT(*) FROM mission */
    public int count() {
        return kvData.size();
    }

    /** SELECT * FROM mission WHERE id = ? */
    public MissionTable selectById(int id) {
        return kvData.get(id);
    }

    /** SELECT EXISTS(SELECT 1 FROM mission WHERE id = ?) */
    public boolean exists(int id) {
        return kvData.containsKey(id);
    }

    /** SELECT * FROM mission WHERE id IN (?, ?, ...) */
    public List<MissionTable> selectByIds(List<Integer> ids) {
        List<MissionTable> result = new ArrayList<>(ids.size());
        for (Integer id : ids) {
            MissionTable row = kvData.get(id);
            if (row != null) {
                result.add(row);
            }
        }
        return result;
    }

    /** Returns the primary-key map */
    public Map<Integer, MissionTable> dataMap() {
        return Collections.unmodifiableMap(kvData);
    }




    /** SELECT * FROM mission WHERE ? IN (condition_id) */
    public List<MissionTable> selectWhereInCondition_id(int key) {
        return idxCondition_id.getOrDefault(key, Collections.emptyList());
    }

    /** SELECT * FROM mission WHERE ? IN (next_mission_id) */
    public List<MissionTable> selectWhereInNext_mission_id(int key) {
        return idxNext_mission_id.getOrDefault(key, Collections.emptyList());
    }

    /** SELECT * FROM mission WHERE ? IN (target_count) */
    public List<MissionTable> selectWhereInTarget_count(int key) {
        return idxTarget_count.getOrDefault(key, Collections.emptyList());
    }



}