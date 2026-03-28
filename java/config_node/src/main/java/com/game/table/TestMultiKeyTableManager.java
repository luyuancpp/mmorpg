
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

    public void load(String configDir) throws Exception {
        String json = Files.readString(Path.of(configDir, "testmultikey.json"));
        TestMultiKeyTableData.Builder builder = TestMultiKeyTableData.newBuilder();
        JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
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



}