
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

    public void load(String configDir, boolean useBinary) throws Exception {
        CooldownTableData.Builder builder = CooldownTableData.newBuilder();
        if (useBinary) {
            byte[] raw = Files.readAllBytes(Path.of(configDir, "cooldown.pb"));
            builder.mergeFrom(raw);
        } else {
            String json = Files.readString(Path.of(configDir, "cooldown.json"));
            JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
        }
        this.data = builder.build();

        for (CooldownTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
        }
    }

    /** SELECT * FROM cooldown */
    public CooldownTableData selectAll() {
        return data;
    }

    /** SELECT COUNT(*) FROM cooldown */
    public int count() {
        return kvData.size();
    }

    /** SELECT * FROM cooldown WHERE id = ? */
    public CooldownTable selectById(int id) {
        return kvData.get(id);
    }

    /** SELECT EXISTS(SELECT 1 FROM cooldown WHERE id = ?) */
    public boolean exists(int id) {
        return kvData.containsKey(id);
    }

    /** SELECT * FROM cooldown WHERE id IN (?, ?, ...) */
    public List<CooldownTable> selectByIds(List<Integer> ids) {
        List<CooldownTable> result = new ArrayList<>(ids.size());
        for (Integer id : ids) {
            CooldownTable row = kvData.get(id);
            if (row != null) {
                result.add(row);
            }
        }
        return result;
    }

    /** Returns the primary-key map */
    public Map<Integer, CooldownTable> dataMap() {
        return Collections.unmodifiableMap(kvData);
    }






}