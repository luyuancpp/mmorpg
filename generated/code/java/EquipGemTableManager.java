
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
 * Auto-generated config manager for EquipGem.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public class EquipGemTableManager {

    private static final EquipGemTableManager INSTANCE = new EquipGemTableManager();

    /**
     * Internal snapshot holding all parsed data and indices.
     * load() builds a new snapshot and swaps it in, replacing the old one.
     */
    private static class Snapshot {
        final EquipGemTableData data;
        final Map<Integer, EquipGemTable> kvData;




        final Map<Integer, List<EquipGemTable>> idxId;

        final Map<Integer, List<EquipGemTable>> idxAttrId;


        Snapshot(EquipGemTableData data,
                 Map<Integer, EquipGemTable> kvData,
                 Map<Integer, List<EquipGemTable>> idxId,
                 Map<Integer, List<EquipGemTable>> idxAttrId) {
            this.data = data;
            this.kvData = kvData;
            this.idxId = idxId;
            this.idxAttrId = idxAttrId;
        }
    }

    private Snapshot snapshot = new Snapshot(
            EquipGemTableData.getDefaultInstance(),
            Collections.emptyMap(),
            Collections.emptyMap(),
            Collections.emptyMap()
    );

    public static EquipGemTableManager getInstance() {
        return INSTANCE;
    }

    public void load(String configDir, boolean useBinary) throws Exception {
        EquipGemTableData.Builder builder = EquipGemTableData.newBuilder();
        if (useBinary) {
            byte[] raw = Files.readAllBytes(Path.of(configDir, "equipgem.pb"));
            builder.mergeFrom(raw);
        } else {
            String json = Files.readString(Path.of(configDir, "equipgem.json"));
            JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
        }
        EquipGemTableData data = builder.build();

        Map<Integer, EquipGemTable> kvData = new HashMap<>(data.getDataCount());
        Map<Integer, List<EquipGemTable>> idxId = new HashMap<>();
        Map<Integer, List<EquipGemTable>> idxAttrId = new HashMap<>();

        for (EquipGemTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
            idxId.computeIfAbsent(row.getId(), k -> new ArrayList<>()).add(row);
            idxAttrId.computeIfAbsent(row.getAttrId(), k -> new ArrayList<>()).add(row);
        }

        this.snapshot = new Snapshot(data, kvData, idxId, idxAttrId);
    }

    public EquipGemTableData findAll() {
        return snapshot.data;
    }

    /** 热更契约：返回的对象属于当前快照，调用方**只存 id**，不要长期持有引用。 */
    public EquipGemTable findById(int id) {
        return snapshot.kvData.get(id);
    }

    public Map<Integer, EquipGemTable> getKvData() {
        return Collections.unmodifiableMap(snapshot.kvData);
    }







    public List<EquipGemTable> getById(int key) {
        return snapshot.idxId.getOrDefault(key, Collections.emptyList());
    }

    public List<EquipGemTable> getByAttrId(int key) {
        return snapshot.idxAttrId.getOrDefault(key, Collections.emptyList());
    }



    // FK: id → Item.id

    // FK: attr_id → EquipAttribute.id


    // ---- Exists ----

    public boolean exists(int id) {
        return snapshot.kvData.containsKey(id);
    }



    // ---- Count ----

    public int count() {
        return snapshot.kvData.size();
    }




    public int countByIdIndex(int key) {
        return snapshot.idxId.getOrDefault(key, Collections.emptyList()).size();
    }

    public int countByAttrIdIndex(int key) {
        return snapshot.idxAttrId.getOrDefault(key, Collections.emptyList()).size();
    }


    // ---- FindByIds (IN) ----

    public List<EquipGemTable> findByIds(List<Integer> ids) {
        Snapshot snap = this.snapshot;
        List<EquipGemTable> result = new ArrayList<>(ids.size());
        for (int id : ids) {
            EquipGemTable row = snap.kvData.get(id);
            if (row != null) { result.add(row); }
        }
        return result;
    }

    // ---- RandOne ----

    public EquipGemTable randOne() {
        Snapshot snap = this.snapshot;
        if (snap.data == null || snap.data.getDataCount() == 0) return null;
        int idx = ThreadLocalRandom.current().nextInt(snap.data.getDataCount());
        return snap.data.getData(idx);
    }

    // ---- Where / First ----

    public List<EquipGemTable> where(Predicate<EquipGemTable> pred) {
        Snapshot snap = this.snapshot;
        List<EquipGemTable> result = new ArrayList<>();
        for (EquipGemTable row : snap.data.getDataList()) {
            if (pred.test(row)) { result.add(row); }
        }
        return result;
    }

    public EquipGemTable first(Predicate<EquipGemTable> pred) {
        Snapshot snap = this.snapshot;
        for (EquipGemTable row : snap.data.getDataList()) {
            if (pred.test(row)) { return row; }
        }
        return null;
    }
}