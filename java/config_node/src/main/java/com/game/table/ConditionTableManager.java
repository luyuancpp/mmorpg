
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
 * Auto-generated config manager for Condition.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public class ConditionTableManager {

    private static final ConditionTableManager INSTANCE = new ConditionTableManager();

    private ConditionTableData data;
    private final Map<Integer, ConditionTable> kvData = new HashMap<>();


    private final Map<Integer, List<ConditionTable>> idxCondition1 = new HashMap<>();

    private final Map<Integer, List<ConditionTable>> idxCondition2 = new HashMap<>();

    private final Map<Integer, List<ConditionTable>> idxCondition3 = new HashMap<>();

    private final Map<Integer, List<ConditionTable>> idxCondition4 = new HashMap<>();


    public static ConditionTableManager getInstance() {
        return INSTANCE;
    }

    public void load(String configDir) throws Exception {
        String json = Files.readString(Path.of(configDir, "condition.json"));
        ConditionTableData.Builder builder = ConditionTableData.newBuilder();
        JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
        this.data = builder.build();

        for (ConditionTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
            for (Integer elem : row.getCondition1List()) {
                idxCondition1.computeIfAbsent(elem, k -> new ArrayList<>()).add(row);
            }
            for (Integer elem : row.getCondition2List()) {
                idxCondition2.computeIfAbsent(elem, k -> new ArrayList<>()).add(row);
            }
            for (Integer elem : row.getCondition3List()) {
                idxCondition3.computeIfAbsent(elem, k -> new ArrayList<>()).add(row);
            }
            for (Integer elem : row.getCondition4List()) {
                idxCondition4.computeIfAbsent(elem, k -> new ArrayList<>()).add(row);
            }
        }
    }

    public ConditionTableData getAll() {
        return data;
    }

    public ConditionTable getById(int id) {
        return kvData.get(id);
    }

    public Map<Integer, ConditionTable> getKvData() {
        return Collections.unmodifiableMap(kvData);
    }




    public List<ConditionTable> getByCondition1Index(int key) {
        return idxCondition1.getOrDefault(key, Collections.emptyList());
    }

    public List<ConditionTable> getByCondition2Index(int key) {
        return idxCondition2.getOrDefault(key, Collections.emptyList());
    }

    public List<ConditionTable> getByCondition3Index(int key) {
        return idxCondition3.getOrDefault(key, Collections.emptyList());
    }

    public List<ConditionTable> getByCondition4Index(int key) {
        return idxCondition4.getOrDefault(key, Collections.emptyList());
    }



}