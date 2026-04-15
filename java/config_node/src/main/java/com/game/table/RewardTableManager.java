
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
 * Auto-generated config manager for Reward.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public class RewardTableManager {

    private static final RewardTableManager INSTANCE = new RewardTableManager();

    private RewardTableData data;
    private final Map<Integer, RewardTable> kvData = new HashMap<>();



    public static RewardTableManager getInstance() {
        return INSTANCE;
    }

    public void load(String configDir, boolean useBinary) throws Exception {
        RewardTableData.Builder builder = RewardTableData.newBuilder();
        if (useBinary) {
            byte[] raw = Files.readAllBytes(Path.of(configDir, "reward.pb"));
            builder.mergeFrom(raw);
        } else {
            String json = Files.readString(Path.of(configDir, "reward.json"));
            JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
        }
        this.data = builder.build();

        for (RewardTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
        }
    }

    /** SELECT * FROM reward */
    public RewardTableData selectAll() {
        return data;
    }

    /** SELECT COUNT(*) FROM reward */
    public int count() {
        return kvData.size();
    }

    /** SELECT * FROM reward WHERE id = ? */
    public RewardTable selectById(int id) {
        return kvData.get(id);
    }

    /** SELECT EXISTS(SELECT 1 FROM reward WHERE id = ?) */
    public boolean exists(int id) {
        return kvData.containsKey(id);
    }

    /** SELECT * FROM reward WHERE id IN (?, ?, ...) */
    public List<RewardTable> selectByIds(List<Integer> ids) {
        List<RewardTable> result = new ArrayList<>(ids.size());
        for (Integer id : ids) {
            RewardTable row = kvData.get(id);
            if (row != null) {
                result.add(row);
            }
        }
        return result;
    }

    /** Returns the primary-key map */
    public Map<Integer, RewardTable> dataMap() {
        return Collections.unmodifiableMap(kvData);
    }






}