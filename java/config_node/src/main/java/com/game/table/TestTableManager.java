
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
 * Auto-generated config manager for Test.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public class TestTableManager {

    private static final TestTableManager INSTANCE = new TestTableManager();

    private TestTableData data;
    private final Map<Integer, TestTable> kvData = new HashMap<>();


    private final Map<Integer, List<TestTable>> idxEffect = new HashMap<>();


    public static TestTableManager getInstance() {
        return INSTANCE;
    }

    public void load(String configDir, boolean useBinary) throws Exception {
        TestTableData.Builder builder = TestTableData.newBuilder();
        if (useBinary) {
            byte[] raw = Files.readAllBytes(Path.of(configDir, "test.pb"));
            builder.mergeFrom(raw);
        } else {
            String json = Files.readString(Path.of(configDir, "test.json"));
            JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
        }
        this.data = builder.build();

        for (TestTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
            for (Integer elem : row.getEffectList()) {
                idxEffect.computeIfAbsent(elem, k -> new ArrayList<>()).add(row);
            }
        }
    }

    public TestTableData getAll() {
        return data;
    }

    public TestTable getById(int id) {
        return kvData.get(id);
    }

    public Map<Integer, TestTable> getKvData() {
        return Collections.unmodifiableMap(kvData);
    }




    public List<TestTable> getByEffectIndex(int key) {
        return idxEffect.getOrDefault(key, Collections.emptyList());
    }




    // ---- Has / Exists ----

    public boolean hasId(int id) {
        return kvData.containsKey(id);
    }



    // ---- Len / Count ----

    public int size() {
        return kvData.size();
    }



    public int countByEffectIndex(int key) {
        return idxEffect.getOrDefault(key, Collections.emptyList()).size();
    }


    // ---- Batch Lookup (IN) ----

    public List<TestTable> getByIds(List<Integer> ids) {
        List<TestTable> result = new ArrayList<>(ids.size());
        for (int id : ids) {
            TestTable row = kvData.get(id);
            if (row != null) { result.add(row); }
        }
        return result;
    }

    // ---- Random ----

    public TestTable getRandom() {
        if (data == null || data.getDataCount() == 0) return null;
        int idx = ThreadLocalRandom.current().nextInt(data.getDataCount());
        return data.getData(idx);
    }

    // ---- Filter / FindFirst ----

    public List<TestTable> filter(Predicate<TestTable> pred) {
        List<TestTable> result = new ArrayList<>();
        for (TestTable row : data.getDataList()) {
            if (pred.test(row)) { result.add(row); }
        }
        return result;
    }

    public TestTable findFirst(Predicate<TestTable> pred) {
        for (TestTable row : data.getDataList()) {
            if (pred.test(row)) { return row; }
        }
        return null;
    }
}