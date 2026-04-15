
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
 * Auto-generated config manager for Mirror.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public class MirrorTableManager {

    private static final MirrorTableManager INSTANCE = new MirrorTableManager();

    private MirrorTableData data;
    private final Map<Integer, MirrorTable> kvData = new HashMap<>();



    public static MirrorTableManager getInstance() {
        return INSTANCE;
    }

    public void load(String configDir, boolean useBinary) throws Exception {
        MirrorTableData.Builder builder = MirrorTableData.newBuilder();
        if (useBinary) {
            byte[] raw = Files.readAllBytes(Path.of(configDir, "mirror.pb"));
            builder.mergeFrom(raw);
        } else {
            String json = Files.readString(Path.of(configDir, "mirror.json"));
            JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
        }
        this.data = builder.build();

        for (MirrorTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
        }
    }

    public MirrorTableData getAll() {
        return data;
    }

    public MirrorTable getById(int id) {
        return kvData.get(id);
    }

    public Map<Integer, MirrorTable> getKvData() {
        return Collections.unmodifiableMap(kvData);
    }






    // FK: scene_id → BaseScene.id

    // FK: main_scene_id → World.id

}