
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
 * Auto-generated config manager for GlobalVariable.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public class GlobalVariableTableManager {

    private static final GlobalVariableTableManager INSTANCE = new GlobalVariableTableManager();

    private GlobalVariableTableData data;
    private final Map<Integer, GlobalVariableTable> kvData = new HashMap<>();



    public static GlobalVariableTableManager getInstance() {
        return INSTANCE;
    }

    public void load(String configDir, boolean useBinary) throws Exception {
        GlobalVariableTableData.Builder builder = GlobalVariableTableData.newBuilder();
        if (useBinary) {
            byte[] raw = Files.readAllBytes(Path.of(configDir, "globalvariable.pb"));
            builder.mergeFrom(raw);
        } else {
            String json = Files.readString(Path.of(configDir, "globalvariable.json"));
            JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
        }
        this.data = builder.build();

        for (GlobalVariableTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
        }
    }

    public GlobalVariableTableData findAll() {
        return data;
    }

    public GlobalVariableTable findById(int id) {
        return kvData.get(id);
    }

    public Map<Integer, GlobalVariableTable> getKvData() {
        return Collections.unmodifiableMap(kvData);
    }







    // ---- Exists ----

    public boolean exists(int id) {
        return kvData.containsKey(id);
    }



    // ---- Count ----

    public int count() {
        return kvData.size();
    }




    // ---- FindByIds (IN) ----

    public List<GlobalVariableTable> findByIds(List<Integer> ids) {
        List<GlobalVariableTable> result = new ArrayList<>(ids.size());
        for (int id : ids) {
            GlobalVariableTable row = kvData.get(id);
            if (row != null) { result.add(row); }
        }
        return result;
    }

    // ---- RandOne ----

    public GlobalVariableTable randOne() {
        if (data == null || data.getDataCount() == 0) return null;
        int idx = ThreadLocalRandom.current().nextInt(data.getDataCount());
        return data.getData(idx);
    }

    // ---- Where / First ----

    public List<GlobalVariableTable> where(Predicate<GlobalVariableTable> pred) {
        List<GlobalVariableTable> result = new ArrayList<>();
        for (GlobalVariableTable row : data.getDataList()) {
            if (pred.test(row)) { result.add(row); }
        }
        return result;
    }

    public GlobalVariableTable first(Predicate<GlobalVariableTable> pred) {
        for (GlobalVariableTable row : data.getDataList()) {
            if (pred.test(row)) { return row; }
        }
        return null;
    }
}