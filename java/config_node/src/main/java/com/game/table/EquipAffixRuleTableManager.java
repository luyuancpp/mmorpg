
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
 * Auto-generated config manager for EquipAffixRule.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public class EquipAffixRuleTableManager {

    private static final EquipAffixRuleTableManager INSTANCE = new EquipAffixRuleTableManager();

    /**
     * Internal snapshot holding all parsed data and indices.
     * load() builds a new snapshot and swaps it in, replacing the old one.
     */
    private static class Snapshot {
        final EquipAffixRuleTableData data;
        final Map<Integer, EquipAffixRuleTable> kvData;





        Snapshot(EquipAffixRuleTableData data,
                 Map<Integer, EquipAffixRuleTable> kvData) {
            this.data = data;
            this.kvData = kvData;
        }
    }

    private Snapshot snapshot = new Snapshot(
            EquipAffixRuleTableData.getDefaultInstance(),
            Collections.emptyMap()
    );

    public static EquipAffixRuleTableManager getInstance() {
        return INSTANCE;
    }

    public void load(String configDir, boolean useBinary) throws Exception {
        EquipAffixRuleTableData.Builder builder = EquipAffixRuleTableData.newBuilder();
        if (useBinary) {
            byte[] raw = Files.readAllBytes(Path.of(configDir, "equipaffixrule.pb"));
            builder.mergeFrom(raw);
        } else {
            String json = Files.readString(Path.of(configDir, "equipaffixrule.json"));
            JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
        }
        EquipAffixRuleTableData data = builder.build();

        Map<Integer, EquipAffixRuleTable> kvData = new HashMap<>(data.getDataCount());

        for (EquipAffixRuleTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
        }

        this.snapshot = new Snapshot(data, kvData);
    }

    public EquipAffixRuleTableData findAll() {
        return snapshot.data;
    }

    /** 热更契约：返回的对象属于当前快照，调用方**只存 id**，不要长期持有引用。 */
    public EquipAffixRuleTable findById(int id) {
        return snapshot.kvData.get(id);
    }

    public Map<Integer, EquipAffixRuleTable> getKvData() {
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

    public List<EquipAffixRuleTable> findByIds(List<Integer> ids) {
        Snapshot snap = this.snapshot;
        List<EquipAffixRuleTable> result = new ArrayList<>(ids.size());
        for (int id : ids) {
            EquipAffixRuleTable row = snap.kvData.get(id);
            if (row != null) { result.add(row); }
        }
        return result;
    }

    // ---- RandOne ----

    public EquipAffixRuleTable randOne() {
        Snapshot snap = this.snapshot;
        if (snap.data == null || snap.data.getDataCount() == 0) return null;
        int idx = ThreadLocalRandom.current().nextInt(snap.data.getDataCount());
        return snap.data.getData(idx);
    }

    // ---- Where / First ----

    public List<EquipAffixRuleTable> where(Predicate<EquipAffixRuleTable> pred) {
        Snapshot snap = this.snapshot;
        List<EquipAffixRuleTable> result = new ArrayList<>();
        for (EquipAffixRuleTable row : snap.data.getDataList()) {
            if (pred.test(row)) { result.add(row); }
        }
        return result;
    }

    public EquipAffixRuleTable first(Predicate<EquipAffixRuleTable> pred) {
        Snapshot snap = this.snapshot;
        for (EquipAffixRuleTable row : snap.data.getDataList()) {
            if (pred.test(row)) { return row; }
        }
        return null;
    }
}