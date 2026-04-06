
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
 * Auto-generated config manager for Scene.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public class SceneTableManager {

    private static final SceneTableManager INSTANCE = new SceneTableManager();

    private SceneTableData data;
    private final Map<Integer, SceneTable> kvData = new HashMap<>();



    public static SceneTableManager getInstance() {
        return INSTANCE;
    }

    public void load(String configDir, boolean useBinary) throws Exception {
        SceneTableData.Builder builder = SceneTableData.newBuilder();
        if (useBinary) {
            byte[] raw = Files.readAllBytes(Path.of(configDir, "scene.pb"));
            builder.mergeFrom(raw);
        } else {
            String json = Files.readString(Path.of(configDir, "scene.json"));
            JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
        }
        this.data = builder.build();

        for (SceneTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
        }
    }

    public SceneTableData getAll() {
        return data;
    }

    public SceneTable getById(int id) {
        return kvData.get(id);
    }

    public Map<Integer, SceneTable> getKvData() {
        return Collections.unmodifiableMap(kvData);
    }






}