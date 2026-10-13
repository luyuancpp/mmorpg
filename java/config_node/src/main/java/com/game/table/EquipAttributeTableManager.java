
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
 * Auto-generated config manager for EquipAttribute.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public class EquipAttributeTableManager {

    private static final EquipAttributeTableManager INSTANCE = new EquipAttributeTableManager();

    /**
     * Internal snapshot holding all parsed data and indices.
     * load() builds a new snapshot and swaps it in, replacing the old one.
     */
    private static class Snapshot {
        final EquipAttributeTableData data;
        final Map<Integer, EquipAttributeTable> kvData;





        Snapshot(EquipAttributeTableData data,
                 Map<Integer, EquipAttributeTable> kvData) {
            this.data = data;
            this.kvData = kvData;
        }
    }

    private Snapshot snapshot = new Snapshot(
            EquipAttributeTableData.getDefaultInstance(),
            Collections.emptyMap()
    );

    public static EquipAttributeTableManager getInstance() {
        return INSTANCE;
    }

    public void load(String configDir, boolean useBinary) throws Exception {
        EquipAttributeTableData.Builder builder = EquipAttributeTableData.newBuilder();
        if (useBinary) {
            byte[] raw = Files.readAllBytes(Path.of(configDir, "equipattribute.pb"));
            builder.mergeFrom(raw);
        } else {
            String json = Files.readString(Path.of(configDir, "equipattribute.json"));
            JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
        }
        EquipAttributeTableData data = builder.build();

        Map<Integer, EquipAttributeTable> kvData = new HashMap<>(data.getDataCount());

        for (EquipAttributeTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
        }

        this.snapshot = new Snapshot(data, kvData);
    }

    public EquipAttributeTableData findAll() {
        return snapshot.data;
    }

    /** 热更契约：返回的对象属于当前快照，调用方**只存 id**，不要长期持有引用。 */
    public EquipAttributeTable findById(int id) {
        return snapshot.kvData.get(id);
    }

    public Map<Integer, EquipAttributeTable> getKvData() {
        return Collections.unmodifiableMap(snapshot.kvData);
    }










    // ---- Exists ----

    public boolean exists(int id) {
        return snapshot.kvData.containsKey(id);
    }



    // ---- Count ----

    public int count() {
        return snapshot.kvData.size();
    }





    // ---- FindByIds (IN) ----

    public List<EquipAttributeTable> findByIds(List<Integer> ids) {
        Snapshot snap = this.snapshot;
        List<EquipAttributeTable> result = new ArrayList<>(ids.size());
        for (int id : ids) {
            EquipAttributeTable row = snap.kvData.get(id);
            if (row != null) { result.add(row); }
        }
        return result;
    }

    // ---- RandOne ----

    public EquipAttributeTable randOne() {
        Snapshot snap = this.snapshot;
        if (snap.data == null || snap.data.getDataCount() == 0) return null;
        int idx = ThreadLocalRandom.current().nextInt(snap.data.getDataCount());
        return snap.data.getData(idx);
    }

    // ---- Where / First ----

    public List<EquipAttributeTable> where(Predicate<EquipAttributeTable> pred) {
        Snapshot snap = this.snapshot;
        List<EquipAttributeTable> result = new ArrayList<>();
        for (EquipAttributeTable row : snap.data.getDataList()) {
            if (pred.test(row)) { result.add(row); }
        }
        return result;
    }

    public EquipAttributeTable first(Predicate<EquipAttributeTable> pred) {
        Snapshot snap = this.snapshot;
        for (EquipAttributeTable row : snap.data.getDataList()) {
            if (pred.test(row)) { return row; }
        }
        return null;
    }
}