
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
 * Auto-generated config manager for MainScene.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public class MainSceneTableManager {

    private static final MainSceneTableManager INSTANCE = new MainSceneTableManager();

    private MainSceneTableData data;
    private final Map<Integer, MainSceneTable> kvData = new HashMap<>();



    public static MainSceneTableManager getInstance() {
        return INSTANCE;
    }

    public void load(String configDir, boolean useBinary) throws Exception {
        MainSceneTableData.Builder builder = MainSceneTableData.newBuilder();
        if (useBinary) {
            byte[] raw = Files.readAllBytes(Path.of(configDir, "mainscene.pb"));
            builder.mergeFrom(raw);
        } else {
            String json = Files.readString(Path.of(configDir, "mainscene.json"));
            JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
        }
        this.data = builder.build();

        for (MainSceneTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
        }
    }

    public MainSceneTableData getAll() {
        return data;
    }

    public MainSceneTable getById(int id) {
        return kvData.get(id);
    }

    public Map<Integer, MainSceneTable> getKvData() {
        return Collections.unmodifiableMap(kvData);
    }






}