
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
 * Auto-generated config manager for Dungeon.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public class DungeonTableManager {

    private static final DungeonTableManager INSTANCE = new DungeonTableManager();

    private DungeonTableData data;
    private final Map<Integer, DungeonTable> kvData = new HashMap<>();



    public static DungeonTableManager getInstance() {
        return INSTANCE;
    }

    public void load(String configDir, boolean useBinary) throws Exception {
        DungeonTableData.Builder builder = DungeonTableData.newBuilder();
        if (useBinary) {
            byte[] raw = Files.readAllBytes(Path.of(configDir, "dungeon.pb"));
            builder.mergeFrom(raw);
        } else {
            String json = Files.readString(Path.of(configDir, "dungeon.json"));
            JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
        }
        this.data = builder.build();

        for (DungeonTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
        }
    }

    public DungeonTableData findAll() {
        return data;
    }

    public DungeonTable findById(int id) {
        return kvData.get(id);
    }

    public Map<Integer, DungeonTable> getKvData() {
        return Collections.unmodifiableMap(kvData);
    }






    // FK: scene_id → BaseScene.id


    // ---- Exists ----

    public boolean exists(int id) {
        return kvData.containsKey(id);
    }



    // ---- Count ----

    public int count() {
        return kvData.size();
    }




    // ---- FindByIds (IN) ----

    public List<DungeonTable> findByIds(List<Integer> ids) {
        List<DungeonTable> result = new ArrayList<>(ids.size());
        for (int id : ids) {
            DungeonTable row = kvData.get(id);
            if (row != null) { result.add(row); }
        }
        return result;
    }

    // ---- RandOne ----

    public DungeonTable randOne() {
        if (data == null || data.getDataCount() == 0) return null;
        int idx = ThreadLocalRandom.current().nextInt(data.getDataCount());
        return data.getData(idx);
    }

    // ---- Where / First ----

    public List<DungeonTable> where(Predicate<DungeonTable> pred) {
        List<DungeonTable> result = new ArrayList<>();
        for (DungeonTable row : data.getDataList()) {
            if (pred.test(row)) { result.add(row); }
        }
        return result;
    }

    public DungeonTable first(Predicate<DungeonTable> pred) {
        for (DungeonTable row : data.getDataList()) {
            if (pred.test(row)) { return row; }
        }
        return null;
    }
}