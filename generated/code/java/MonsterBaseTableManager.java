
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
 * Auto-generated config manager for MonsterBase.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public class MonsterBaseTableManager {

    private static final MonsterBaseTableManager INSTANCE = new MonsterBaseTableManager();

    private MonsterBaseTableData data;
    private final Map<Integer, MonsterBaseTable> kvData = new HashMap<>();



    public static MonsterBaseTableManager getInstance() {
        return INSTANCE;
    }

    public void load(String configDir) throws Exception {
        String json = Files.readString(Path.of(configDir, "monsterbase.json"));
        MonsterBaseTableData.Builder builder = MonsterBaseTableData.newBuilder();
        JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
        this.data = builder.build();

        for (MonsterBaseTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
        }
    }

    public MonsterBaseTableData getAll() {
        return data;
    }

    public MonsterBaseTable getById(int id) {
        return kvData.get(id);
    }

    public Map<Integer, MonsterBaseTable> getKvData() {
        return Collections.unmodifiableMap(kvData);
    }






}