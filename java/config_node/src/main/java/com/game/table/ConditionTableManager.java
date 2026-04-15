
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

    public void load(String configDir, boolean useBinary) throws Exception {
        ConditionTableData.Builder builder = ConditionTableData.newBuilder();
        if (useBinary) {
            byte[] raw = Files.readAllBytes(Path.of(configDir, "condition.pb"));
            builder.mergeFrom(raw);
        } else {
            String json = Files.readString(Path.of(configDir, "condition.json"));
            JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
        }
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

    /** SELECT * FROM condition */
    public ConditionTableData selectAll() {
        return data;
    }

    /** SELECT COUNT(*) FROM condition */
    public int count() {
        return kvData.size();
    }

    /** SELECT * FROM condition WHERE id = ? */
    public ConditionTable selectById(int id) {
        return kvData.get(id);
    }

    /** SELECT EXISTS(SELECT 1 FROM condition WHERE id = ?) */
    public boolean exists(int id) {
        return kvData.containsKey(id);
    }

    /** SELECT * FROM condition WHERE id IN (?, ?, ...) */
    public List<ConditionTable> selectByIds(List<Integer> ids) {
        List<ConditionTable> result = new ArrayList<>(ids.size());
        for (Integer id : ids) {
            ConditionTable row = kvData.get(id);
            if (row != null) {
                result.add(row);
            }
        }
        return result;
    }

    /** Returns the primary-key map */
    public Map<Integer, ConditionTable> dataMap() {
        return Collections.unmodifiableMap(kvData);
    }




    /** SELECT * FROM condition WHERE ? IN (condition1) */
    public List<ConditionTable> selectWhereInCondition1(int key) {
        return idxCondition1.getOrDefault(key, Collections.emptyList());
    }

    /** SELECT * FROM condition WHERE ? IN (condition2) */
    public List<ConditionTable> selectWhereInCondition2(int key) {
        return idxCondition2.getOrDefault(key, Collections.emptyList());
    }

    /** SELECT * FROM condition WHERE ? IN (condition3) */
    public List<ConditionTable> selectWhereInCondition3(int key) {
        return idxCondition3.getOrDefault(key, Collections.emptyList());
    }

    /** SELECT * FROM condition WHERE ? IN (condition4) */
    public List<ConditionTable> selectWhereInCondition4(int key) {
        return idxCondition4.getOrDefault(key, Collections.emptyList());
    }



}