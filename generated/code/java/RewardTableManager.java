
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

    public void load(String configDir) throws Exception {
        String json = Files.readString(Path.of(configDir, "reward.json"));
        RewardTableData.Builder builder = RewardTableData.newBuilder();
        JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
        this.data = builder.build();

        for (RewardTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
        }
    }

    public RewardTableData getAll() {
        return data;
    }

    public RewardTable getById(int id) {
        return kvData.get(id);
    }

    public Map<Integer, RewardTable> getKvData() {
        return Collections.unmodifiableMap(kvData);
    }






}