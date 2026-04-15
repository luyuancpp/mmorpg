
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

    /**
     * Internal snapshot holding all parsed data and indices.
     * load() builds a new snapshot and swaps it in, replacing the old one.
     */
    private static class Snapshot {
        final TestMultiKeyTableData data;
        final Map<Integer, TestMultiKeyTable> kvData;

        final Map<String, TestMultiKeyTable> kvString_keyData;

        final Map<Integer, TestMultiKeyTable> kvUint32_keyData;

        final Map<Integer, TestMultiKeyTable> kvInt32_keyData;


        final Map<String, List<TestMultiKeyTable>> kvM_string_keyData;

        final Map<Integer, List<TestMultiKeyTable>> kvM_uint32_keyData;

        final Map<Integer, List<TestMultiKeyTable>> kvM_int32_keyData;


        final Map<Integer, List<TestMultiKeyTable>> idxEffect;

        final Map<Integer, List<TestMultiKeyTable>> idxTest_refs;


        final Map<Integer, List<TestMultiKeyTable>> idxLevel;

        final Map<Integer, List<TestMultiKeyTable>> idxTestRef;


        Snapshot(TestMultiKeyTableData data,
                 Map<Integer, TestMultiKeyTable> kvData,
                 Map<String, TestMultiKeyTable> kvString_keyData,
                 Map<Integer, TestMultiKeyTable> kvUint32_keyData,
                 Map<Integer, TestMultiKeyTable> kvInt32_keyData,
                 Map<String, List<TestMultiKeyTable>> kvM_string_keyData,
                 Map<Integer, List<TestMultiKeyTable>> kvM_uint32_keyData,
                 Map<Integer, List<TestMultiKeyTable>> kvM_int32_keyData,
                 Map<Integer, List<TestMultiKeyTable>> idxEffect,
                 Map<Integer, List<TestMultiKeyTable>> idxTest_refs,
                 Map<Integer, List<TestMultiKeyTable>> idxLevel,
                 Map<Integer, List<TestMultiKeyTable>> idxTestRef) {
            this.data = data;
            this.kvData = kvData;
            this.kvString_keyData = kvString_keyData;
            this.kvUint32_keyData = kvUint32_keyData;
            this.kvInt32_keyData = kvInt32_keyData;
            this.kvM_string_keyData = kvM_string_keyData;
            this.kvM_uint32_keyData = kvM_uint32_keyData;
            this.kvM_int32_keyData = kvM_int32_keyData;
            this.idxEffect = idxEffect;
            this.idxTest_refs = idxTest_refs;
            this.idxLevel = idxLevel;
            this.idxTestRef = idxTestRef;
        }
    }

    private Snapshot snapshot = new Snapshot(
            TestMultiKeyTableData.getDefaultInstance(),
            Collections.emptyMap(),
            Collections.emptyMap(),
            Collections.emptyMap(),
            Collections.emptyMap(),
            Collections.emptyMap(),
            Collections.emptyMap(),
            Collections.emptyMap(),
            Collections.emptyMap(),
            Collections.emptyMap(),
            Collections.emptyMap(),
            Collections.emptyMap()
    );

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
        TestMultiKeyTableData data = builder.build();

        Map<Integer, TestMultiKeyTable> kvData = new HashMap<>(data.getDataCount());
        Map<String, TestMultiKeyTable> kvString_keyData = new HashMap<>(data.getDataCount());
        Map<Integer, TestMultiKeyTable> kvUint32_keyData = new HashMap<>(data.getDataCount());
        Map<Integer, TestMultiKeyTable> kvInt32_keyData = new HashMap<>(data.getDataCount());
        Map<String, List<TestMultiKeyTable>> kvM_string_keyData = new HashMap<>();
        Map<Integer, List<TestMultiKeyTable>> kvM_uint32_keyData = new HashMap<>();
        Map<Integer, List<TestMultiKeyTable>> kvM_int32_keyData = new HashMap<>();
        Map<Integer, List<TestMultiKeyTable>> idxEffect = new HashMap<>();
        Map<Integer, List<TestMultiKeyTable>> idxTest_refs = new HashMap<>();
        Map<Integer, List<TestMultiKeyTable>> idxLevel = new HashMap<>();
        Map<Integer, List<TestMultiKeyTable>> idxTestRef = new HashMap<>();

        for (TestMultiKeyTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
            kvString_keyData.put(row.getStringKey(), row);
            kvUint32_keyData.put(row.getUint32Key(), row);
            kvInt32_keyData.put(row.getInt32Key(), row);
            kvM_string_keyData.computeIfAbsent(row.getMStringKey(), k -> new ArrayList<>()).add(row);
            kvM_uint32_keyData.computeIfAbsent(row.getMUint32Key(), k -> new ArrayList<>()).add(row);
            kvM_int32_keyData.computeIfAbsent(row.getMInt32Key(), k -> new ArrayList<>()).add(row);
            for (Integer elem : row.getEffectList()) {
                idxEffect.computeIfAbsent(elem, k -> new ArrayList<>()).add(row);
            }
            for (Integer elem : row.getTestRefsList()) {
                idxTest_refs.computeIfAbsent(elem, k -> new ArrayList<>()).add(row);
            }
            idxLevel.computeIfAbsent(row.getLevel(), k -> new ArrayList<>()).add(row);
            idxTestRef.computeIfAbsent(row.getTestRef(), k -> new ArrayList<>()).add(row);
        }

        this.snapshot = new Snapshot(data, kvData, kvString_keyData, kvUint32_keyData, kvInt32_keyData, kvM_string_keyData, kvM_uint32_keyData, kvM_int32_keyData, idxEffect, idxTest_refs, idxLevel, idxTestRef);
    }

    public TestMultiKeyTableData findAll() {
        return snapshot.data;
    }

    public TestMultiKeyTable findById(int id) {
        return snapshot.kvData.get(id);
    }

    public Map<Integer, TestMultiKeyTable> getKvData() {
        return Collections.unmodifiableMap(snapshot.kvData);
    }


    public TestMultiKeyTable findByString_key(String key) {
        return snapshot.kvString_keyData.get(key);
    }

    public TestMultiKeyTable findByUint32_key(int key) {
        return snapshot.kvUint32_keyData.get(key);
    }

    public TestMultiKeyTable findByInt32_key(int key) {
        return snapshot.kvInt32_keyData.get(key);
    }


    public List<TestMultiKeyTable> findByM_string_key(String key) {
        return snapshot.kvM_string_keyData.getOrDefault(key, Collections.emptyList());
    }

    public List<TestMultiKeyTable> findByM_uint32_key(int key) {
        return snapshot.kvM_uint32_keyData.getOrDefault(key, Collections.emptyList());
    }

    public List<TestMultiKeyTable> findByM_int32_key(int key) {
        return snapshot.kvM_int32_keyData.getOrDefault(key, Collections.emptyList());
    }



    public List<TestMultiKeyTable> findByEffectIndex(int key) {
        return snapshot.idxEffect.getOrDefault(key, Collections.emptyList());
    }

    public List<TestMultiKeyTable> findByTest_refsIndex(int key) {
        return snapshot.idxTest_refs.getOrDefault(key, Collections.emptyList());
    }



    public List<TestMultiKeyTable> getByLevel(int key) {
        return snapshot.idxLevel.getOrDefault(key, Collections.emptyList());
    }

    public List<TestMultiKeyTable> getByTestRef(int key) {
        return snapshot.idxTestRef.getOrDefault(key, Collections.emptyList());
    }



    // FK: test_ref → Test.id


    // ---- Exists ----

    public boolean exists(int id) {
        return snapshot.kvData.containsKey(id);
    }


    public boolean existsByString_key(String key) {
        return snapshot.kvString_keyData.containsKey(key);
    }

    public boolean existsByUint32_key(int key) {
        return snapshot.kvUint32_keyData.containsKey(key);
    }

    public boolean existsByInt32_key(int key) {
        return snapshot.kvInt32_keyData.containsKey(key);
    }


    // ---- Count ----

    public int count() {
        return snapshot.kvData.size();
    }


    public int countByM_string_key(String key) {
        return snapshot.kvM_string_keyData.getOrDefault(key, Collections.emptyList()).size();
    }

    public int countByM_uint32_key(int key) {
        return snapshot.kvM_uint32_keyData.getOrDefault(key, Collections.emptyList()).size();
    }

    public int countByM_int32_key(int key) {
        return snapshot.kvM_int32_keyData.getOrDefault(key, Collections.emptyList()).size();
    }


    public int countByEffectIndex(int key) {
        return snapshot.idxEffect.getOrDefault(key, Collections.emptyList()).size();
    }

    public int countByTest_refsIndex(int key) {
        return snapshot.idxTest_refs.getOrDefault(key, Collections.emptyList()).size();
    }


    public int countByLevelIndex(int key) {
        return snapshot.idxLevel.getOrDefault(key, Collections.emptyList()).size();
    }

    public int countByTestRefIndex(int key) {
        return snapshot.idxTestRef.getOrDefault(key, Collections.emptyList()).size();
    }


    // ---- FindByIds (IN) ----

    public List<TestMultiKeyTable> findByIds(List<Integer> ids) {
        Snapshot snap = this.snapshot;
        List<TestMultiKeyTable> result = new ArrayList<>(ids.size());
        for (int id : ids) {
            TestMultiKeyTable row = snap.kvData.get(id);
            if (row != null) { result.add(row); }
        }
        return result;
    }

    // ---- RandOne ----

    public TestMultiKeyTable randOne() {
        Snapshot snap = this.snapshot;
        if (snap.data == null || snap.data.getDataCount() == 0) return null;
        int idx = ThreadLocalRandom.current().nextInt(snap.data.getDataCount());
        return snap.data.getData(idx);
    }

    // ---- Where / First ----

    public List<TestMultiKeyTable> where(Predicate<TestMultiKeyTable> pred) {
        Snapshot snap = this.snapshot;
        List<TestMultiKeyTable> result = new ArrayList<>();
        for (TestMultiKeyTable row : snap.data.getDataList()) {
            if (pred.test(row)) { result.add(row); }
        }
        return result;
    }

    public TestMultiKeyTable first(Predicate<TestMultiKeyTable> pred) {
        Snapshot snap = this.snapshot;
        for (TestMultiKeyTable row : snap.data.getDataList()) {
            if (pred.test(row)) { return row; }
        }
        return null;
    }
}