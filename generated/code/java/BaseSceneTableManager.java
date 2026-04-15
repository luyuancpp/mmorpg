
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
 * Auto-generated config manager for BaseScene.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public class BaseSceneTableManager {

    private static final BaseSceneTableManager INSTANCE = new BaseSceneTableManager();

    private BaseSceneTableData data;
    private final Map<Integer, BaseSceneTable> kvData = new HashMap<>();



    public static BaseSceneTableManager getInstance() {
        return INSTANCE;
    }

    public void load(String configDir, boolean useBinary) throws Exception {
        BaseSceneTableData.Builder builder = BaseSceneTableData.newBuilder();
        if (useBinary) {
            byte[] raw = Files.readAllBytes(Path.of(configDir, "basescene.pb"));
            builder.mergeFrom(raw);
        } else {
            String json = Files.readString(Path.of(configDir, "basescene.json"));
            JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
        }
        this.data = builder.build();

        for (BaseSceneTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
        }
    }

    public BaseSceneTableData getAll() {
        return data;
    }

    public BaseSceneTable getById(int id) {
        return kvData.get(id);
    }

    public Map<Integer, BaseSceneTable> getKvData() {
        return Collections.unmodifiableMap(kvData);
    }






}