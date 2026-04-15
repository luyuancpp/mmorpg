
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

    public WorldTableData getAll() {
        return data;
    }

    public WorldTable getById(int id) {
        return kvData.get(id);
    }

    public Map<Integer, WorldTable> getKvData() {
        return Collections.unmodifiableMap(kvData);
    }






    // FK: scene_id → BaseScene.id


    // ---- Has / Exists ----

    public boolean hasId(int id) {
        return kvData.containsKey(id);
    }



    // ---- Len / Count ----

    public int size() {
        return kvData.size();
    }




    // ---- Batch Lookup (IN) ----

    public List<WorldTable> getByIds(List<Integer> ids) {
        List<WorldTable> result = new ArrayList<>(ids.size());
        for (int id : ids) {
            WorldTable row = kvData.get(id);
            if (row != null) { result.add(row); }
        }
        return result;
    }

    // ---- Random ----

    public WorldTable getRandom() {
        if (data == null || data.getDataCount() == 0) return null;
        int idx = ThreadLocalRandom.current().nextInt(data.getDataCount());
        return data.getData(idx);
    }

    // ---- Filter / FindFirst ----

    public List<WorldTable> filter(Predicate<WorldTable> pred) {
        List<WorldTable> result = new ArrayList<>();
        for (WorldTable row : data.getDataList()) {
            if (pred.test(row)) { result.add(row); }
        }
        return result;
    }

    public WorldTable findFirst(Predicate<WorldTable> pred) {
        for (WorldTable row : data.getDataList()) {
            if (pred.test(row)) { return row; }
        }
        return null;
    }
}