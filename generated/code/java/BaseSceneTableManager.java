
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
 * Auto-generated config manager for BaseScene.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public class BaseSceneTableManager {

    private static final BaseSceneTableManager INSTANCE = new BaseSceneTableManager();

    private BaseSceneTableData data;
    private final Map<Integer, BaseSceneTable> kvData = new HashMap<>();



    public static BaseSceneTableManager getInstance() {
        return INSTANCE;
    }

    public void load(String configDir, boolean useBinary) throws Exception {
        BaseSceneTableData.Builder builder = BaseSceneTableData.newBuilder();
        if (useBinary) {
            byte[] raw = Files.readAllBytes(Path.of(configDir, "basescene.pb"));
            builder.mergeFrom(raw);
        } else {
            String json = Files.readString(Path.of(configDir, "basescene.json"));
            JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
        }
        this.data = builder.build();

        for (BaseSceneTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
        }
    }

    public BaseSceneTableData findAll() {
        return data;
    }

    public BaseSceneTable findById(int id) {
        return kvData.get(id);
    }

    public Map<Integer, BaseSceneTable> getKvData() {
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

    public List<BaseSceneTable> findByIds(List<Integer> ids) {
        List<BaseSceneTable> result = new ArrayList<>(ids.size());
        for (int id : ids) {
            BaseSceneTable row = kvData.get(id);
            if (row != null) { result.add(row); }
        }
        return result;
    }

    // ---- RandOne ----

    public BaseSceneTable randOne() {
        if (data == null || data.getDataCount() == 0) return null;
        int idx = ThreadLocalRandom.current().nextInt(data.getDataCount());
        return data.getData(idx);
    }

    // ---- Where / First ----

    public List<BaseSceneTable> where(Predicate<BaseSceneTable> pred) {
        List<BaseSceneTable> result = new ArrayList<>();
        for (BaseSceneTable row : data.getDataList()) {
            if (pred.test(row)) { result.add(row); }
        }
        return result;
    }

    public BaseSceneTable first(Predicate<BaseSceneTable> pred) {
        for (BaseSceneTable row : data.getDataList()) {
            if (pred.test(row)) { return row; }
        }
        return null;
    }
}