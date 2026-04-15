
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

    /** SELECT * FROM testmultikey */
    public TestMultiKeyTableData selectAll() {
        return data;
    }

    /** SELECT COUNT(*) FROM testmultikey */
    public int count() {
        return kvData.size();
    }

    /** SELECT * FROM testmultikey WHERE id = ? */
    public TestMultiKeyTable selectById(int id) {
        return kvData.get(id);
    }

    /** SELECT EXISTS(SELECT 1 FROM testmultikey WHERE id = ?) */
    public boolean exists(int id) {
        return kvData.containsKey(id);
    }

    /** SELECT * FROM testmultikey WHERE id IN (?, ?, ...) */
    public List<TestMultiKeyTable> selectByIds(List<Integer> ids) {
        List<TestMultiKeyTable> result = new ArrayList<>(ids.size());
        for (Integer id : ids) {
            TestMultiKeyTable row = kvData.get(id);
            if (row != null) {
                result.add(row);
            }
        }
        return result;
    }

    /** Returns the primary-key map */
    public Map<Integer, TestMultiKeyTable> dataMap() {
        return Collections.unmodifiableMap(kvData);
    }


    /** SELECT * FROM testmultikey WHERE string_key = ? */
    public TestMultiKeyTable selectByString_key(String key) {
        return kvString_keyData.get(key);
    }

    /** SELECT * FROM testmultikey WHERE uint32_key = ? */
    public TestMultiKeyTable selectByUint32_key(int key) {
        return kvUint32_keyData.get(key);
    }

    /** SELECT * FROM testmultikey WHERE int32_key = ? */
    public TestMultiKeyTable selectByInt32_key(int key) {
        return kvInt32_keyData.get(key);
    }

    /** SELECT * FROM testmultikey WHERE m_string_key = ? */
    public TestMultiKeyTable selectByM_string_key(String key) {
        return kvM_string_keyData.get(key);
    }

    /** SELECT * FROM testmultikey WHERE m_uint32_key = ? */
    public TestMultiKeyTable selectByM_uint32_key(int key) {
        return kvM_uint32_keyData.get(key);
    }

    /** SELECT * FROM testmultikey WHERE m_int32_key = ? */
    public TestMultiKeyTable selectByM_int32_key(int key) {
        return kvM_int32_keyData.get(key);
    }



    /** SELECT * FROM testmultikey WHERE ? IN (effect) */
    public List<TestMultiKeyTable> selectWhereInEffect(int key) {
        return idxEffect.getOrDefault(key, Collections.emptyList());
    }



}