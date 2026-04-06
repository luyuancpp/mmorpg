
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



}