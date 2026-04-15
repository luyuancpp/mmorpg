
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
 * Auto-generated config manager for Cooldown.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public class CooldownTableManager {

    private static final CooldownTableManager INSTANCE = new CooldownTableManager();

    private CooldownTableData data;
    private final Map<Integer, CooldownTable> kvData = new HashMap<>();



    public static CooldownTableManager getInstance() {
        return INSTANCE;
    }

    public void load(String configDir, boolean useBinary) throws Exception {
        CooldownTableData.Builder builder = CooldownTableData.newBuilder();
        if (useBinary) {
            byte[] raw = Files.readAllBytes(Path.of(configDir, "cooldown.pb"));
            builder.mergeFrom(raw);
        } else {
            String json = Files.readString(Path.of(configDir, "cooldown.json"));
            JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
        }
        this.data = builder.build();

        for (CooldownTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
        }
    }

    public CooldownTableData findAll() {
        return data;
    }

    public CooldownTable findById(int id) {
        return kvData.get(id);
    }

    public Map<Integer, CooldownTable> getKvData() {
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

    public List<CooldownTable> findByIds(List<Integer> ids) {
        List<CooldownTable> result = new ArrayList<>(ids.size());
        for (int id : ids) {
            CooldownTable row = kvData.get(id);
            if (row != null) { result.add(row); }
        }
        return result;
    }

    // ---- RandOne ----

    public CooldownTable randOne() {
        if (data == null || data.getDataCount() == 0) return null;
        int idx = ThreadLocalRandom.current().nextInt(data.getDataCount());
        return data.getData(idx);
    }

    // ---- Where / First ----

    public List<CooldownTable> where(Predicate<CooldownTable> pred) {
        List<CooldownTable> result = new ArrayList<>();
        for (CooldownTable row : data.getDataList()) {
            if (pred.test(row)) { result.add(row); }
        }
        return result;
    }

    public CooldownTable first(Predicate<CooldownTable> pred) {
        for (CooldownTable row : data.getDataList()) {
            if (pred.test(row)) { return row; }
        }
        return null;
    }
}