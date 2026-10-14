
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
 * Auto-generated config manager for EquipAttributeCap.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public class EquipAttributeCapTableManager {

    private static final EquipAttributeCapTableManager INSTANCE = new EquipAttributeCapTableManager();

    /**
     * Internal snapshot holding all parsed data and indices.
     * load() builds a new snapshot and swaps it in, replacing the old one.
     */
    private static class Snapshot {
        final EquipAttributeCapTableData data;
        final Map<Integer, EquipAttributeCapTable> kvData;




        final Map<Integer, List<EquipAttributeCapTable>> idxAttrId;


        Snapshot(EquipAttributeCapTableData data,
                 Map<Integer, EquipAttributeCapTable> kvData,
                 Map<Integer, List<EquipAttributeCapTable>> idxAttrId) {
            this.data = data;
            this.kvData = kvData;
            this.idxAttrId = idxAttrId;
        }
    }

    private Snapshot snapshot = new Snapshot(
            EquipAttributeCapTableData.getDefaultInstance(),
            Collections.emptyMap(),
            Collections.emptyMap()
    );

    public static EquipAttributeCapTableManager getInstance() {
        return INSTANCE;
    }

    public void load(String configDir, boolean useBinary) throws Exception {
        EquipAttributeCapTableData.Builder builder = EquipAttributeCapTableData.newBuilder();
        if (useBinary) {
            byte[] raw = Files.readAllBytes(Path.of(configDir, "equipattributecap.pb"));
            builder.mergeFrom(raw);
        } else {
            String json = Files.readString(Path.of(configDir, "equipattributecap.json"));
            JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
        }
        EquipAttributeCapTableData data = builder.build();

        Map<Integer, EquipAttributeCapTable> kvData = new HashMap<>(data.getDataCount());
        Map<Integer, List<EquipAttributeCapTable>> idxAttrId = new HashMap<>();

        for (EquipAttributeCapTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
            idxAttrId.computeIfAbsent(row.getAttrId(), k -> new ArrayList<>()).add(row);
        }

        this.snapshot = new Snapshot(data, kvData, idxAttrId);
    }

    public EquipAttributeCapTableData findAll() {
        return snapshot.data;
    }

    /** 热更契约：返回的对象属于当前快照，调用方**只存 id**，不要长期持有引用。 */
    public EquipAttributeCapTable findById(int id) {
        return snapshot.kvData.get(id);
    }

    public Map<Integer, EquipAttributeCapTable> getKvData() {
        return Collections.unmodifiableMap(snapshot.kvData);
    }







    public List<EquipAttributeCapTable> getByAttrId(int key) {
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




    public int countByAttrIdIndex(int key) {
        return snapshot.idxAttrId.getOrDefault(key, Collections.emptyList()).size();
    }


    // ---- FindByIds (IN) ----

    public List<EquipAttributeCapTable> findByIds(List<Integer> ids) {
        Snapshot snap = this.snapshot;
        List<EquipAttributeCapTable> result = new ArrayList<>(ids.size());
        for (int id : ids) {
            EquipAttributeCapTable row = snap.kvData.get(id);
            if (row != null) { result.add(row); }
        }
        return result;
    }

    // ---- RandOne ----

    public EquipAttributeCapTable randOne() {
        Snapshot snap = this.snapshot;
        if (snap.data == null || snap.data.getDataCount() == 0) return null;
        int idx = ThreadLocalRandom.current().nextInt(snap.data.getDataCount());
        return snap.data.getData(idx);
    }

    // ---- Where / First ----

    public List<EquipAttributeCapTable> where(Predicate<EquipAttributeCapTable> pred) {
        Snapshot snap = this.snapshot;
        List<EquipAttributeCapTable> result = new ArrayList<>();
        for (EquipAttributeCapTable row : snap.data.getDataList()) {
            if (pred.test(row)) { result.add(row); }
        }
        return result;
    }

    public EquipAttributeCapTable first(Predicate<EquipAttributeCapTable> pred) {
        Snapshot snap = this.snapshot;
        for (EquipAttributeCapTable row : snap.data.getDataList()) {
            if (pred.test(row)) { return row; }
        }
        return null;
    }
}