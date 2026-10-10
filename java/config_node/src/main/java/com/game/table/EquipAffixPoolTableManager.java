
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
 * Auto-generated config manager for EquipAffixPool.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public class EquipAffixPoolTableManager {

    private static final EquipAffixPoolTableManager INSTANCE = new EquipAffixPoolTableManager();

    /**
     * Internal snapshot holding all parsed data and indices.
     * load() builds a new snapshot and swaps it in, replacing the old one.
     */
    private static class Snapshot {
        final EquipAffixPoolTableData data;
        final Map<Integer, EquipAffixPoolTable> kvData;




        final Map<Integer, List<EquipAffixPoolTable>> idxPoolId;

        final Map<Integer, List<EquipAffixPoolTable>> idxAttrId;


        Snapshot(EquipAffixPoolTableData data,
                 Map<Integer, EquipAffixPoolTable> kvData,
                 Map<Integer, List<EquipAffixPoolTable>> idxPoolId,
                 Map<Integer, List<EquipAffixPoolTable>> idxAttrId) {
            this.data = data;
            this.kvData = kvData;
            this.idxPoolId = idxPoolId;
            this.idxAttrId = idxAttrId;
        }
    }

    private Snapshot snapshot = new Snapshot(
            EquipAffixPoolTableData.getDefaultInstance(),
            Collections.emptyMap(),
            Collections.emptyMap(),
            Collections.emptyMap()
    );

    public static EquipAffixPoolTableManager getInstance() {
        return INSTANCE;
    }

    public void load(String configDir, boolean useBinary) throws Exception {
        EquipAffixPoolTableData.Builder builder = EquipAffixPoolTableData.newBuilder();
        if (useBinary) {
            byte[] raw = Files.readAllBytes(Path.of(configDir, "equipaffixpool.pb"));
            builder.mergeFrom(raw);
        } else {
            String json = Files.readString(Path.of(configDir, "equipaffixpool.json"));
            JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
        }
        EquipAffixPoolTableData data = builder.build();

        Map<Integer, EquipAffixPoolTable> kvData = new HashMap<>(data.getDataCount());
        Map<Integer, List<EquipAffixPoolTable>> idxPoolId = new HashMap<>();
        Map<Integer, List<EquipAffixPoolTable>> idxAttrId = new HashMap<>();

        for (EquipAffixPoolTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
            idxPoolId.computeIfAbsent(row.getPoolId(), k -> new ArrayList<>()).add(row);
            idxAttrId.computeIfAbsent(row.getAttrId(), k -> new ArrayList<>()).add(row);
        }

        this.snapshot = new Snapshot(data, kvData, idxPoolId, idxAttrId);
    }

    public EquipAffixPoolTableData findAll() {
        return snapshot.data;
    }

    /** 热更契约：返回的对象属于当前快照，调用方**只存 id**，不要长期持有引用。 */
    public EquipAffixPoolTable findById(int id) {
        return snapshot.kvData.get(id);
    }

    public Map<Integer, EquipAffixPoolTable> getKvData() {
        return Collections.unmodifiableMap(snapshot.kvData);
    }







    public List<EquipAffixPoolTable> getByPoolId(int key) {
        return snapshot.idxPoolId.getOrDefault(key, Collections.emptyList());
    }

    public List<EquipAffixPoolTable> getByAttrId(int key) {
        return snapshot.idxAttrId.getOrDefault(key, Collections.emptyList());
    }



    // FK: attr_id → EquipAttribute.id


    // ---- Exists ----

    public boolean exists(int id) {
        return snapshot.kvData.containsKey(id);
    }



    // ---- Count ----

    public int count() {
        return snapshot.kvData.size();
    }




    public int countByPoolIdIndex(int key) {
        return snapshot.idxPoolId.getOrDefault(key, Collections.emptyList()).size();
    }

    public int countByAttrIdIndex(int key) {
        return snapshot.idxAttrId.getOrDefault(key, Collections.emptyList()).size();
    }


    // ---- FindByIds (IN) ----

    public List<EquipAffixPoolTable> findByIds(List<Integer> ids) {
        Snapshot snap = this.snapshot;
        List<EquipAffixPoolTable> result = new ArrayList<>(ids.size());
        for (int id : ids) {
            EquipAffixPoolTable row = snap.kvData.get(id);
            if (row != null) { result.add(row); }
        }
        return result;
    }

    // ---- RandOne ----

    public EquipAffixPoolTable randOne() {
        Snapshot snap = this.snapshot;
        if (snap.data == null || snap.data.getDataCount() == 0) return null;
        int idx = ThreadLocalRandom.current().nextInt(snap.data.getDataCount());
        return snap.data.getData(idx);
    }

    // ---- Where / First ----

    public List<EquipAffixPoolTable> where(Predicate<EquipAffixPoolTable> pred) {
        Snapshot snap = this.snapshot;
        List<EquipAffixPoolTable> result = new ArrayList<>();
        for (EquipAffixPoolTable row : snap.data.getDataList()) {
            if (pred.test(row)) { result.add(row); }
        }
        return result;
    }

    public EquipAffixPoolTable first(Predicate<EquipAffixPoolTable> pred) {
        Snapshot snap = this.snapshot;
        for (EquipAffixPoolTable row : snap.data.getDataList()) {
            if (pred.test(row)) { return row; }
        }
        return null;
    }
}