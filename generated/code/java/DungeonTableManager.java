
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

    public DungeonTableData getAll() {
        return data;
    }

    public DungeonTable getById(int id) {
        return kvData.get(id);
    }

    public Map<Integer, DungeonTable> getKvData() {
        return Collections.unmodifiableMap(kvData);
    }






    // FK: scene_id → BaseScene.id

}