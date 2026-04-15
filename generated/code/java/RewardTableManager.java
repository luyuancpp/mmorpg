
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
 * Auto-generated config manager for Reward.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public class RewardTableManager {

    private static final RewardTableManager INSTANCE = new RewardTableManager();

    private RewardTableData data;
    private final Map<Integer, RewardTable> kvData = new HashMap<>();



    public static RewardTableManager getInstance() {
        return INSTANCE;
    }

    public void load(String configDir, boolean useBinary) throws Exception {
        RewardTableData.Builder builder = RewardTableData.newBuilder();
        if (useBinary) {
            byte[] raw = Files.readAllBytes(Path.of(configDir, "reward.pb"));
            builder.mergeFrom(raw);
        } else {
            String json = Files.readString(Path.of(configDir, "reward.json"));
            JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
        }
        this.data = builder.build();

        for (RewardTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
        }
    }

    public RewardTableData findAll() {
        return data;
    }

    public RewardTable findById(int id) {
        return kvData.get(id);
    }

    public Map<Integer, RewardTable> getKvData() {
        return Collections.unmodifiableMap(kvData);
    }







    // ---- Exists ----

    public boolean exists(int id) {
        return kvData.containsKey(id);
    }



    // ---- Count ----

    public int count() {
        return kvData.size();
    }




    // ---- FindByIds (IN) ----

    public List<RewardTable> findByIds(List<Integer> ids) {
        List<RewardTable> result = new ArrayList<>(ids.size());
        for (int id : ids) {
            RewardTable row = kvData.get(id);
            if (row != null) { result.add(row); }
        }
        return result;
    }

    // ---- RandOne ----

    public RewardTable randOne() {
        if (data == null || data.getDataCount() == 0) return null;
        int idx = ThreadLocalRandom.current().nextInt(data.getDataCount());
        return data.getData(idx);
    }

    // ---- Where / First ----

    public List<RewardTable> where(Predicate<RewardTable> pred) {
        List<RewardTable> result = new ArrayList<>();
        for (RewardTable row : data.getDataList()) {
            if (pred.test(row)) { result.add(row); }
        }
        return result;
    }

    public RewardTable first(Predicate<RewardTable> pred) {
        for (RewardTable row : data.getDataList()) {
            if (pred.test(row)) { return row; }
        }
        return null;
    }
}