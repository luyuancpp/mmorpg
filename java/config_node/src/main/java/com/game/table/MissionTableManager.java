
package com.game.table;

import com.google.protobuf.util.JsonFormat;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ThreadLocalRandom;
import java.util.function.Predicate;

/**
 * Auto-generated config manager for Mission.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public class MissionTableManager {

    private static final MissionTableManager INSTANCE = new MissionTableManager();

    /**
     * Internal snapshot holding all parsed data and indices.
     * load() builds a new snapshot and swaps it in, replacing the old one.
     */
    private static class Snapshot {
        final MissionTableData data;
        final Map<Integer, MissionTable> kvData;



        final Map<Integer, List<MissionTable>> idxCondition_id;

        final Map<Integer, List<MissionTable>> idxNext_mission_id;

        final Map<Integer, List<MissionTable>> idxTarget_count;


        Snapshot(MissionTableData data,
                 Map<Integer, MissionTable> kvData,
                 Map<Integer, List<MissionTable>> idxCondition_id,
                 Map<Integer, List<MissionTable>> idxNext_mission_id,
                 Map<Integer, List<MissionTable>> idxTarget_count) {
            this.data = data;
            this.kvData = kvData;
            this.idxCondition_id = idxCondition_id;
            this.idxNext_mission_id = idxNext_mission_id;
            this.idxTarget_count = idxTarget_count;
        }
    }

    private Snapshot snapshot = new Snapshot(
            MissionTableData.getDefaultInstance(),
            Collections.emptyMap(),
            Collections.emptyMap(),
            Collections.emptyMap(),
            Collections.emptyMap()
    );

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
        MissionTableData data = builder.build();

        Map<Integer, MissionTable> kvData = new HashMap<>(data.getDataCount());
        Map<Integer, List<MissionTable>> idxCondition_id = new HashMap<>();
        Map<Integer, List<MissionTable>> idxNext_mission_id = new HashMap<>();
        Map<Integer, List<MissionTable>> idxTarget_count = new HashMap<>();

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

        this.snapshot = new Snapshot(data, kvData, idxCondition_id, idxNext_mission_id, idxTarget_count);
    }

    public MissionTableData findAll() {
        return snapshot.data;
    }

    public MissionTable findById(int id) {
        return snapshot.kvData.get(id);
    }

    public Map<Integer, MissionTable> getKvData() {
        return Collections.unmodifiableMap(snapshot.kvData);
    }





    public List<MissionTable> findByCondition_idIndex(int key) {
        return snapshot.idxCondition_id.getOrDefault(key, Collections.emptyList());
    }

    public List<MissionTable> findByNext_mission_idIndex(int key) {
        return snapshot.idxNext_mission_id.getOrDefault(key, Collections.emptyList());
    }

    public List<MissionTable> findByTarget_countIndex(int key) {
        return snapshot.idxTarget_count.getOrDefault(key, Collections.emptyList());
    }




    // ---- Exists ----

    public boolean exists(int id) {
        return snapshot.kvData.containsKey(id);
    }



    // ---- Count ----

    public int count() {
        return snapshot.kvData.size();
    }



    public int countByCondition_idIndex(int key) {
        return snapshot.idxCondition_id.getOrDefault(key, Collections.emptyList()).size();
    }

    public int countByNext_mission_idIndex(int key) {
        return snapshot.idxNext_mission_id.getOrDefault(key, Collections.emptyList()).size();
    }

    public int countByTarget_countIndex(int key) {
        return snapshot.idxTarget_count.getOrDefault(key, Collections.emptyList()).size();
    }


    // ---- FindByIds (IN) ----

    public List<MissionTable> findByIds(List<Integer> ids) {
        Snapshot snap = this.snapshot;
        List<MissionTable> result = new ArrayList<>(ids.size());
        for (int id : ids) {
            MissionTable row = snap.kvData.get(id);
            if (row != null) { result.add(row); }
        }
        return result;
    }

    // ---- RandOne ----

    public MissionTable randOne() {
        Snapshot snap = this.snapshot;
        if (snap.data == null || snap.data.getDataCount() == 0) return null;
        int idx = ThreadLocalRandom.current().nextInt(snap.data.getDataCount());
        return snap.data.getData(idx);
    }

    // ---- Where / First ----

    public List<MissionTable> where(Predicate<MissionTable> pred) {
        Snapshot snap = this.snapshot;
        List<MissionTable> result = new ArrayList<>();
        for (MissionTable row : snap.data.getDataList()) {
            if (pred.test(row)) { result.add(row); }
        }
        return result;
    }

    public MissionTable first(Predicate<MissionTable> pred) {
        Snapshot snap = this.snapshot;
        for (MissionTable row : snap.data.getDataList()) {
            if (pred.test(row)) { return row; }
        }
        return null;
    }
}