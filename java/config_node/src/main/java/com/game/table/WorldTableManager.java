
package com.game.table;

import com.google.protobuf.util.JsonFormat;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

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

    /** SELECT * FROM world */
    public WorldTableData selectAll() {
        return data;
    }

    /** SELECT COUNT(*) FROM world */
    public int count() {
        return kvData.size();
    }

    /** SELECT * FROM world WHERE id = ? */
    public WorldTable selectById(int id) {
        return kvData.get(id);
    }

    /** SELECT EXISTS(SELECT 1 FROM world WHERE id = ?) */
    public boolean exists(int id) {
        return kvData.containsKey(id);
    }

    /** SELECT * FROM world WHERE id IN (?, ?, ...) */
    public List<WorldTable> selectByIds(List<Integer> ids) {
        List<WorldTable> result = new ArrayList<>(ids.size());
        for (Integer id : ids) {
            WorldTable row = kvData.get(id);
            if (row != null) {
                result.add(row);
            }
        }
        return result;
    }

    /** Returns the primary-key map */
    public Map<Integer, WorldTable> dataMap() {
        return Collections.unmodifiableMap(kvData);
    }






    // FK: scene_id -> BaseScene.id

}