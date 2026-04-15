
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
 * Auto-generated config manager for Monster.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public class MonsterTableManager {

    private static final MonsterTableManager INSTANCE = new MonsterTableManager();

    private MonsterTableData data;
    private final Map<Integer, MonsterTable> kvData = new HashMap<>();



    public static MonsterTableManager getInstance() {
        return INSTANCE;
    }

    public void load(String configDir, boolean useBinary) throws Exception {
        MonsterTableData.Builder builder = MonsterTableData.newBuilder();
        if (useBinary) {
            byte[] raw = Files.readAllBytes(Path.of(configDir, "monster.pb"));
            builder.mergeFrom(raw);
        } else {
            String json = Files.readString(Path.of(configDir, "monster.json"));
            JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
        }
        this.data = builder.build();

        for (MonsterTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
        }
    }

    public MonsterTableData findAll() {
        return data;
    }

    public MonsterTable findById(int id) {
        return kvData.get(id);
    }

    public Map<Integer, MonsterTable> getKvData() {
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

    public List<MonsterTable> findByIds(List<Integer> ids) {
        List<MonsterTable> result = new ArrayList<>(ids.size());
        for (int id : ids) {
            MonsterTable row = kvData.get(id);
            if (row != null) { result.add(row); }
        }
        return result;
    }

    // ---- RandOne ----

    public MonsterTable randOne() {
        if (data == null || data.getDataCount() == 0) return null;
        int idx = ThreadLocalRandom.current().nextInt(data.getDataCount());
        return data.getData(idx);
    }

    // ---- Where / First ----

    public List<MonsterTable> where(Predicate<MonsterTable> pred) {
        List<MonsterTable> result = new ArrayList<>();
        for (MonsterTable row : data.getDataList()) {
            if (pred.test(row)) { result.add(row); }
        }
        return result;
    }

    public MonsterTable first(Predicate<MonsterTable> pred) {
        for (MonsterTable row : data.getDataList()) {
            if (pred.test(row)) { return row; }
        }
        return null;
    }
}