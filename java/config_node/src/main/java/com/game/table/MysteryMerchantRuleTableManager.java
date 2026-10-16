
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
 * Auto-generated config manager for MysteryMerchantRule.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public class MysteryMerchantRuleTableManager {

    private static final MysteryMerchantRuleTableManager INSTANCE = new MysteryMerchantRuleTableManager();

    /**
     * Internal snapshot holding all parsed data and indices.
     * load() builds a new snapshot and swaps it in, replacing the old one.
     */
    private static class Snapshot {
        final MysteryMerchantRuleTableData data;
        final Map<Integer, MysteryMerchantRuleTable> kvData;





        Snapshot(MysteryMerchantRuleTableData data,
                 Map<Integer, MysteryMerchantRuleTable> kvData) {
            this.data = data;
            this.kvData = kvData;
        }
    }

    private Snapshot snapshot = new Snapshot(
            MysteryMerchantRuleTableData.getDefaultInstance(),
            Collections.emptyMap()
    );

    public static MysteryMerchantRuleTableManager getInstance() {
        return INSTANCE;
    }

    public void load(String configDir, boolean useBinary) throws Exception {
        MysteryMerchantRuleTableData.Builder builder = MysteryMerchantRuleTableData.newBuilder();
        if (useBinary) {
            byte[] raw = Files.readAllBytes(Path.of(configDir, "mysterymerchantrule.pb"));
            builder.mergeFrom(raw);
        } else {
            String json = Files.readString(Path.of(configDir, "mysterymerchantrule.json"));
            JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
        }
        MysteryMerchantRuleTableData data = builder.build();

        Map<Integer, MysteryMerchantRuleTable> kvData = new HashMap<>(data.getDataCount());

        for (MysteryMerchantRuleTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
        }

        this.snapshot = new Snapshot(data, kvData);
    }

    public MysteryMerchantRuleTableData findAll() {
        return snapshot.data;
    }

    /** 热更契约：返回的对象属于当前快照，调用方**只存 id**，不要长期持有引用。 */
    public MysteryMerchantRuleTable findById(int id) {
        return snapshot.kvData.get(id);
    }

    public Map<Integer, MysteryMerchantRuleTable> getKvData() {
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

    public List<MysteryMerchantRuleTable> findByIds(List<Integer> ids) {
        Snapshot snap = this.snapshot;
        List<MysteryMerchantRuleTable> result = new ArrayList<>(ids.size());
        for (int id : ids) {
            MysteryMerchantRuleTable row = snap.kvData.get(id);
            if (row != null) { result.add(row); }
        }
        return result;
    }

    // ---- RandOne ----

    public MysteryMerchantRuleTable randOne() {
        Snapshot snap = this.snapshot;
        if (snap.data == null || snap.data.getDataCount() == 0) return null;
        int idx = ThreadLocalRandom.current().nextInt(snap.data.getDataCount());
        return snap.data.getData(idx);
    }

    // ---- Where / First ----

    public List<MysteryMerchantRuleTable> where(Predicate<MysteryMerchantRuleTable> pred) {
        Snapshot snap = this.snapshot;
        List<MysteryMerchantRuleTable> result = new ArrayList<>();
        for (MysteryMerchantRuleTable row : snap.data.getDataList()) {
            if (pred.test(row)) { result.add(row); }
        }
        return result;
    }

    public MysteryMerchantRuleTable first(Predicate<MysteryMerchantRuleTable> pred) {
        Snapshot snap = this.snapshot;
        for (MysteryMerchantRuleTable row : snap.data.getDataList()) {
            if (pred.test(row)) { return row; }
        }
        return null;
    }
}