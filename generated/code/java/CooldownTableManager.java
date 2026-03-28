
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
 * Auto-generated config manager for Cooldown.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public class CooldownTableManager {

    private static final CooldownTableManager INSTANCE = new CooldownTableManager();

    private CooldownTableData data;
    private final Map<Integer, CooldownTable> kvData = new HashMap<>();



    public static CooldownTableManager getInstance() {
        return INSTANCE;
    }

    public void load(String configDir) throws Exception {
        String json = Files.readString(Path.of(configDir, "cooldown.json"));
        CooldownTableData.Builder builder = CooldownTableData.newBuilder();
        JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
        this.data = builder.build();

        for (CooldownTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
        }
    }

    public CooldownTableData getAll() {
        return data;
    }

    public CooldownTable getById(int id) {
        return kvData.get(id);
    }

    public Map<Integer, CooldownTable> getKvData() {
        return Collections.unmodifiableMap(kvData);
    }






}