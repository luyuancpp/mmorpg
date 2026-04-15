
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
 * Auto-generated config manager for World.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public class WorldTableManager {

    private static final WorldTableManager INSTANCE = new WorldTableManager();

    private WorldTableData data;
    private final Map<Integer, WorldTable> kvData = new HashMap<>();



    public static WorldTableManager getInstance() {
        return INSTANCE;
    }

    public void load(String configDir, boolean useBinary) throws Exception {
        WorldTableData.Builder builder = WorldTableData.newBuilder();
        if (useBinary) {
            byte[] raw = Files.readAllBytes(Path.of(configDir, "world.pb"));
            builder.mergeFrom(raw);
        } else {
            String json = Files.readString(Path.of(configDir, "world.json"));
            JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
        }
        this.data = builder.build();

        for (WorldTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
        }
    }

    public WorldTableData findAll() {
        return data;
    }

    public WorldTable findById(int id) {
        return kvData.get(id);
    }

    public Map<Integer, WorldTable> getKvData() {
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

    public List<WorldTable> findByIds(List<Integer> ids) {
        List<WorldTable> result = new ArrayList<>(ids.size());
        for (int id : ids) {
            WorldTable row = kvData.get(id);
            if (row != null) { result.add(row); }
        }
        return result;
    }

    // ---- RandOne ----

    public WorldTable randOne() {
        if (data == null || data.getDataCount() == 0) return null;
        int idx = ThreadLocalRandom.current().nextInt(data.getDataCount());
        return data.getData(idx);
    }

    // ---- Where / First ----

    public List<WorldTable> where(Predicate<WorldTable> pred) {
        List<WorldTable> result = new ArrayList<>();
        for (WorldTable row : data.getDataList()) {
            if (pred.test(row)) { result.add(row); }
        }
        return result;
    }

    public WorldTable first(Predicate<WorldTable> pred) {
        for (WorldTable row : data.getDataList()) {
            if (pred.test(row)) { return row; }
        }
        return null;
    }
}