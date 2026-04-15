
package com.game.table;

import com.google.protobuf.util.JsonFormat;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ThreadLocalRandom;
import java.util.function.Predicate;

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

    public ConditionTableData findAll() {
        return data;
    }

    public ConditionTable findById(int id) {
        return kvData.get(id);
    }

    public Map<Integer, ConditionTable> getKvData() {
        return Collections.unmodifiableMap(kvData);
    }




    public List<ConditionTable> findByCondition1Index(int key) {
        return idxCondition1.getOrDefault(key, Collections.emptyList());
    }

    public List<ConditionTable> findByCondition2Index(int key) {
        return idxCondition2.getOrDefault(key, Collections.emptyList());
    }

    public List<ConditionTable> findByCondition3Index(int key) {
        return idxCondition3.getOrDefault(key, Collections.emptyList());
    }

    public List<ConditionTable> findByCondition4Index(int key) {
        return idxCondition4.getOrDefault(key, Collections.emptyList());
    }




    // ---- Exists ----

    public boolean exists(int id) {
        return kvData.containsKey(id);
    }



    // ---- Count ----

    public int count() {
        return kvData.size();
    }



    public int countByCondition1Index(int key) {
        return idxCondition1.getOrDefault(key, Collections.emptyList()).size();
    }

    public int countByCondition2Index(int key) {
        return idxCondition2.getOrDefault(key, Collections.emptyList()).size();
    }

    public int countByCondition3Index(int key) {
        return idxCondition3.getOrDefault(key, Collections.emptyList()).size();
    }

    public int countByCondition4Index(int key) {
        return idxCondition4.getOrDefault(key, Collections.emptyList()).size();
    }


    // ---- FindByIds (IN) ----

    public List<ConditionTable> findByIds(List<Integer> ids) {
        List<ConditionTable> result = new ArrayList<>(ids.size());
        for (int id : ids) {
            ConditionTable row = kvData.get(id);
            if (row != null) { result.add(row); }
        }
        return result;
    }

    // ---- RandOne ----

    public ConditionTable randOne() {
        if (data == null || data.getDataCount() == 0) return null;
        int idx = ThreadLocalRandom.current().nextInt(data.getDataCount());
        return data.getData(idx);
    }

    // ---- Where / First ----

    public List<ConditionTable> where(Predicate<ConditionTable> pred) {
        List<ConditionTable> result = new ArrayList<>();
        for (ConditionTable row : data.getDataList()) {
            if (pred.test(row)) { result.add(row); }
        }
        return result;
    }

    public ConditionTable first(Predicate<ConditionTable> pred) {
        for (ConditionTable row : data.getDataList()) {
            if (pred.test(row)) { return row; }
        }
        return null;
    }
}