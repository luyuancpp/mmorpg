
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

    public CooldownTableData getAll() {
        return data;
    }

    public CooldownTable getById(int id) {
        return kvData.get(id);
    }

    public Map<Integer, CooldownTable> getKvData() {
        return Collections.unmodifiableMap(kvData);
    }







    // ---- Has / Exists ----

    public boolean hasId(int id) {
        return kvData.containsKey(id);
    }



    // ---- Len / Count ----

    public int size() {
        return kvData.size();
    }




    // ---- Batch Lookup (IN) ----

    public List<CooldownTable> getByIds(List<Integer> ids) {
        List<CooldownTable> result = new ArrayList<>(ids.size());
        for (int id : ids) {
            CooldownTable row = kvData.get(id);
            if (row != null) { result.add(row); }
        }
        return result;
    }

    // ---- Random ----

    public CooldownTable getRandom() {
        if (data == null || data.getDataCount() == 0) return null;
        int idx = ThreadLocalRandom.current().nextInt(data.getDataCount());
        return data.getData(idx);
    }

    // ---- Filter / FindFirst ----

    public List<CooldownTable> filter(Predicate<CooldownTable> pred) {
        List<CooldownTable> result = new ArrayList<>();
        for (CooldownTable row : data.getDataList()) {
            if (pred.test(row)) { result.add(row); }
        }
        return result;
    }

    public CooldownTable findFirst(Predicate<CooldownTable> pred) {
        for (CooldownTable row : data.getDataList()) {
            if (pred.test(row)) { return row; }
        }
        return null;
    }
}