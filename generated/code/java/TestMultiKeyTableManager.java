
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
 * Auto-generated config manager for TestMultiKey.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public class TestMultiKeyTableManager {

    private static final TestMultiKeyTableManager INSTANCE = new TestMultiKeyTableManager();

    private TestMultiKeyTableData data;
    private final Map<Integer, TestMultiKeyTable> kvData = new HashMap<>();

    private final Map<String, TestMultiKeyTable> kvString_keyData = new HashMap<>();

    private final Map<Integer, TestMultiKeyTable> kvUint32_keyData = new HashMap<>();

    private final Map<Integer, TestMultiKeyTable> kvInt32_keyData = new HashMap<>();

    private final Map<String, TestMultiKeyTable> kvM_string_keyData = new HashMap<>();

    private final Map<Integer, TestMultiKeyTable> kvM_uint32_keyData = new HashMap<>();

    private final Map<Integer, TestMultiKeyTable> kvM_int32_keyData = new HashMap<>();


    private final Map<Integer, List<TestMultiKeyTable>> idxEffect = new HashMap<>();


    public static TestMultiKeyTableManager getInstance() {
        return INSTANCE;
    }

    public void load(String configDir, boolean useBinary) throws Exception {
        TestMultiKeyTableData.Builder builder = TestMultiKeyTableData.newBuilder();
        if (useBinary) {
            byte[] raw = Files.readAllBytes(Path.of(configDir, "testmultikey.pb"));
            builder.mergeFrom(raw);
        } else {
            String json = Files.readString(Path.of(configDir, "testmultikey.json"));
            JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
        }
        this.data = builder.build();

        for (TestMultiKeyTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
            kvString_keyData.put(row.getStringKey(), row);
            kvUint32_keyData.put(row.getUint32Key(), row);
            kvInt32_keyData.put(row.getInt32Key(), row);
            kvM_string_keyData.put(row.getMStringKey(), row);
            kvM_uint32_keyData.put(row.getMUint32Key(), row);
            kvM_int32_keyData.put(row.getMInt32Key(), row);
            for (Integer elem : row.getEffectList()) {
                idxEffect.computeIfAbsent(elem, k -> new ArrayList<>()).add(row);
            }
        }
    }

    public TestMultiKeyTableData getAll() {
        return data;
    }

    public TestMultiKeyTable getById(int id) {
        return kvData.get(id);
    }

    public Map<Integer, TestMultiKeyTable> getKvData() {
        return Collections.unmodifiableMap(kvData);
    }


    public TestMultiKeyTable getByString_key(String key) {
        return kvString_keyData.get(key);
    }

    public TestMultiKeyTable getByUint32_key(int key) {
        return kvUint32_keyData.get(key);
    }

    public TestMultiKeyTable getByInt32_key(int key) {
        return kvInt32_keyData.get(key);
    }

    public TestMultiKeyTable getByM_string_key(String key) {
        return kvM_string_keyData.get(key);
    }

    public TestMultiKeyTable getByM_uint32_key(int key) {
        return kvM_uint32_keyData.get(key);
    }

    public TestMultiKeyTable getByM_int32_key(int key) {
        return kvM_int32_keyData.get(key);
    }



    public List<TestMultiKeyTable> getByEffectIndex(int key) {
        return idxEffect.getOrDefault(key, Collections.emptyList());
    }




    // ---- Has / Exists ----

    public boolean hasId(int id) {
        return kvData.containsKey(id);
    }


    public boolean hasString_key(String key) {
        return kvString_keyData.containsKey(key);
    }

    public boolean hasUint32_key(int key) {
        return kvUint32_keyData.containsKey(key);
    }

    public boolean hasInt32_key(int key) {
        return kvInt32_keyData.containsKey(key);
    }


    // ---- Len / Count ----

    public int size() {
        return kvData.size();
    }


    public int countByM_string_key(String key) {
        return kvM_string_keyData.getOrDefault(key, Collections.emptyList()).size();
    }

    public int countByM_uint32_key(int key) {
        return kvM_uint32_keyData.getOrDefault(key, Collections.emptyList()).size();
    }

    public int countByM_int32_key(int key) {
        return kvM_int32_keyData.getOrDefault(key, Collections.emptyList()).size();
    }


    public int countByEffectIndex(int key) {
        return idxEffect.getOrDefault(key, Collections.emptyList()).size();
    }


    // ---- Batch Lookup (IN) ----

    public List<TestMultiKeyTable> getByIds(List<Integer> ids) {
        List<TestMultiKeyTable> result = new ArrayList<>(ids.size());
        for (int id : ids) {
            TestMultiKeyTable row = kvData.get(id);
            if (row != null) { result.add(row); }
        }
        return result;
    }

    // ---- Random ----

    public TestMultiKeyTable getRandom() {
        if (data == null || data.getDataCount() == 0) return null;
        int idx = ThreadLocalRandom.current().nextInt(data.getDataCount());
        return data.getData(idx);
    }

    // ---- Filter / FindFirst ----

    public List<TestMultiKeyTable> filter(Predicate<TestMultiKeyTable> pred) {
        List<TestMultiKeyTable> result = new ArrayList<>();
        for (TestMultiKeyTable row : data.getDataList()) {
            if (pred.test(row)) { result.add(row); }
        }
        return result;
    }

    public TestMultiKeyTable findFirst(Predicate<TestMultiKeyTable> pred) {
        for (TestMultiKeyTable row : data.getDataList()) {
            if (pred.test(row)) { return row; }
        }
        return null;
    }
}