
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
 * Auto-generated config manager for Buff.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public class BuffTableManager {

    private static final BuffTableManager INSTANCE = new BuffTableManager();

    private BuffTableData data;
    private final Map<Integer, BuffTable> kvData = new HashMap<>();


    private final Map<Double, List<BuffTable>> idxInterval_effect = new HashMap<>();

    private final Map<Integer, List<BuffTable>> idxSub_buff = new HashMap<>();

    private final Map<Integer, List<BuffTable>> idxTarget_sub_buff = new HashMap<>();


    public static BuffTableManager getInstance() {
        return INSTANCE;
    }

    public void load(String configDir, boolean useBinary) throws Exception {
        BuffTableData.Builder builder = BuffTableData.newBuilder();
        if (useBinary) {
            byte[] raw = Files.readAllBytes(Path.of(configDir, "buff.pb"));
            builder.mergeFrom(raw);
        } else {
            String json = Files.readString(Path.of(configDir, "buff.json"));
            JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
        }
        this.data = builder.build();

        for (BuffTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
            for (Double elem : row.getIntervalEffectList()) {
                idxInterval_effect.computeIfAbsent(elem, k -> new ArrayList<>()).add(row);
            }
            for (Integer elem : row.getSubBuffList()) {
                idxSub_buff.computeIfAbsent(elem, k -> new ArrayList<>()).add(row);
            }
            for (Integer elem : row.getTargetSubBuffList()) {
                idxTarget_sub_buff.computeIfAbsent(elem, k -> new ArrayList<>()).add(row);
            }
        }
    }

    public BuffTableData findAll() {
        return data;
    }

    public BuffTable findById(int id) {
        return kvData.get(id);
    }

    public Map<Integer, BuffTable> getKvData() {
        return Collections.unmodifiableMap(kvData);
    }




    public List<BuffTable> findByInterval_effectIndex(double key) {
        return idxInterval_effect.getOrDefault(key, Collections.emptyList());
    }

    public List<BuffTable> findBySub_buffIndex(int key) {
        return idxSub_buff.getOrDefault(key, Collections.emptyList());
    }

    public List<BuffTable> findByTarget_sub_buffIndex(int key) {
        return idxTarget_sub_buff.getOrDefault(key, Collections.emptyList());
    }




    // ---- Exists ----

    public boolean exists(int id) {
        return kvData.containsKey(id);
    }



    // ---- Count ----

    public int count() {
        return kvData.size();
    }



    public int countByInterval_effectIndex(double key) {
        return idxInterval_effect.getOrDefault(key, Collections.emptyList()).size();
    }

    public int countBySub_buffIndex(int key) {
        return idxSub_buff.getOrDefault(key, Collections.emptyList()).size();
    }

    public int countByTarget_sub_buffIndex(int key) {
        return idxTarget_sub_buff.getOrDefault(key, Collections.emptyList()).size();
    }


    // ---- FindByIds (IN) ----

    public List<BuffTable> findByIds(List<Integer> ids) {
        List<BuffTable> result = new ArrayList<>(ids.size());
        for (int id : ids) {
            BuffTable row = kvData.get(id);
            if (row != null) { result.add(row); }
        }
        return result;
    }

    // ---- RandOne ----

    public BuffTable randOne() {
        if (data == null || data.getDataCount() == 0) return null;
        int idx = ThreadLocalRandom.current().nextInt(data.getDataCount());
        return data.getData(idx);
    }

    // ---- Where / First ----

    public List<BuffTable> where(Predicate<BuffTable> pred) {
        List<BuffTable> result = new ArrayList<>();
        for (BuffTable row : data.getDataList()) {
            if (pred.test(row)) { result.add(row); }
        }
        return result;
    }

    public BuffTable first(Predicate<BuffTable> pred) {
        for (BuffTable row : data.getDataList()) {
            if (pred.test(row)) { return row; }
        }
        return null;
    }
}