
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
 * Auto-generated config manager for GlobalVariable.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public class GlobalVariableTableManager {

    private static final GlobalVariableTableManager INSTANCE = new GlobalVariableTableManager();

    private GlobalVariableTableData data;
    private final Map<Integer, GlobalVariableTable> kvData = new HashMap<>();



    public static GlobalVariableTableManager getInstance() {
        return INSTANCE;
    }

    public void load(String configDir, boolean useBinary) throws Exception {
        GlobalVariableTableData.Builder builder = GlobalVariableTableData.newBuilder();
        if (useBinary) {
            byte[] raw = Files.readAllBytes(Path.of(configDir, "globalvariable.pb"));
            builder.mergeFrom(raw);
        } else {
            String json = Files.readString(Path.of(configDir, "globalvariable.json"));
            JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
        }
        this.data = builder.build();

        for (GlobalVariableTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
        }
    }

    public GlobalVariableTableData getAll() {
        return data;
    }

    public GlobalVariableTable getById(int id) {
        return kvData.get(id);
    }

    public Map<Integer, GlobalVariableTable> getKvData() {
        return Collections.unmodifiableMap(kvData);
    }






}