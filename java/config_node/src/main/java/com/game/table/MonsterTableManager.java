
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
 * Auto-generated config manager for Monster.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public class MonsterTableManager {

    private static final MonsterTableManager INSTANCE = new MonsterTableManager();

    private MonsterTableData data;
    private final Map<Integer, MonsterTable> kvData = new HashMap<>();



    public static MonsterTableManager getInstance() {
        return INSTANCE;
    }

    public void load(String configDir, boolean useBinary) throws Exception {
        MonsterTableData.Builder builder = MonsterTableData.newBuilder();
        if (useBinary) {
            byte[] raw = Files.readAllBytes(Path.of(configDir, "monster.pb"));
            builder.mergeFrom(raw);
        } else {
            String json = Files.readString(Path.of(configDir, "monster.json"));
            JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
        }
        this.data = builder.build();

        for (MonsterTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
        }
    }

    /** SELECT * FROM monster */
    public MonsterTableData selectAll() {
        return data;
    }

    /** SELECT COUNT(*) FROM monster */
    public int count() {
        return kvData.size();
    }

    /** SELECT * FROM monster WHERE id = ? */
    public MonsterTable selectById(int id) {
        return kvData.get(id);
    }

    /** SELECT EXISTS(SELECT 1 FROM monster WHERE id = ?) */
    public boolean exists(int id) {
        return kvData.containsKey(id);
    }

    /** SELECT * FROM monster WHERE id IN (?, ?, ...) */
    public List<MonsterTable> selectByIds(List<Integer> ids) {
        List<MonsterTable> result = new ArrayList<>(ids.size());
        for (Integer id : ids) {
            MonsterTable row = kvData.get(id);
            if (row != null) {
                result.add(row);
            }
        }
        return result;
    }

    /** Returns the primary-key map */
    public Map<Integer, MonsterTable> dataMap() {
        return Collections.unmodifiableMap(kvData);
    }






}