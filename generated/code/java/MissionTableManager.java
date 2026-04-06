
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

    public MissionTableData getAll() {
        return data;
    }

    public MissionTable getById(int id) {
        return kvData.get(id);
    }

    public Map<Integer, MissionTable> getKvData() {
        return Collections.unmodifiableMap(kvData);
    }




    public List<MissionTable> getByCondition_idIndex(int key) {
        return idxCondition_id.getOrDefault(key, Collections.emptyList());
    }

    public List<MissionTable> getByNext_mission_idIndex(int key) {
        return idxNext_mission_id.getOrDefault(key, Collections.emptyList());
    }

    public List<MissionTable> getByTarget_countIndex(int key) {
        return idxTarget_count.getOrDefault(key, Collections.emptyList());
    }



}