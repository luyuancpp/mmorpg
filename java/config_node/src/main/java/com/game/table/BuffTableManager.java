
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

    /**
     * Internal snapshot holding all parsed data and indices.
     * load() builds a new snapshot and swaps it in, replacing the old one.
     */
    private static class Snapshot {
        final BuffTableData data;
        final Map<Integer, BuffTable> kvData;



        final Map<Double, List<BuffTable>> idxInterval_effect;

        final Map<Integer, List<BuffTable>> idxSub_buff;

        final Map<Integer, List<BuffTable>> idxTarget_sub_buff;


        Snapshot(BuffTableData data,
                 Map<Integer, BuffTable> kvData,
                 Map<Double, List<BuffTable>> idxInterval_effect,
                 Map<Integer, List<BuffTable>> idxSub_buff,
                 Map<Integer, List<BuffTable>> idxTarget_sub_buff) {
            this.data = data;
            this.kvData = kvData;
            this.idxInterval_effect = idxInterval_effect;
            this.idxSub_buff = idxSub_buff;
            this.idxTarget_sub_buff = idxTarget_sub_buff;
        }
    }

    private Snapshot snapshot = new Snapshot(
            BuffTableData.getDefaultInstance(),
            Collections.emptyMap(),
            Collections.emptyMap(),
            Collections.emptyMap(),
            Collections.emptyMap()
    );

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
        BuffTableData data = builder.build();

        Map<Integer, BuffTable> kvData = new HashMap<>(data.getDataCount());
        Map<Double, List<BuffTable>> idxInterval_effect = new HashMap<>();
        Map<Integer, List<BuffTable>> idxSub_buff = new HashMap<>();
        Map<Integer, List<BuffTable>> idxTarget_sub_buff = new HashMap<>();

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

        this.snapshot = new Snapshot(data, kvData, idxInterval_effect, idxSub_buff, idxTarget_sub_buff);
    }

    public BuffTableData findAll() {
        return snapshot.data;
    }

    public BuffTable findById(int id) {
        return snapshot.kvData.get(id);
    }

    public Map<Integer, BuffTable> getKvData() {
        return Collections.unmodifiableMap(snapshot.kvData);
    }





    public List<BuffTable> findByInterval_effectIndex(double key) {
        return snapshot.idxInterval_effect.getOrDefault(key, Collections.emptyList());
    }

    public List<BuffTable> findBySub_buffIndex(int key) {
        return snapshot.idxSub_buff.getOrDefault(key, Collections.emptyList());
    }

    public List<BuffTable> findByTarget_sub_buffIndex(int key) {
        return snapshot.idxTarget_sub_buff.getOrDefault(key, Collections.emptyList());
    }




    // ---- Exists ----

    public boolean exists(int id) {
        return snapshot.kvData.containsKey(id);
    }



    // ---- Count ----

    public int count() {
        return snapshot.kvData.size();
    }



    public int countByInterval_effectIndex(double key) {
        return snapshot.idxInterval_effect.getOrDefault(key, Collections.emptyList()).size();
    }

    public int countBySub_buffIndex(int key) {
        return snapshot.idxSub_buff.getOrDefault(key, Collections.emptyList()).size();
    }

    public int countByTarget_sub_buffIndex(int key) {
        return snapshot.idxTarget_sub_buff.getOrDefault(key, Collections.emptyList()).size();
    }


    // ---- FindByIds (IN) ----

    public List<BuffTable> findByIds(List<Integer> ids) {
        Snapshot snap = this.snapshot;
        List<BuffTable> result = new ArrayList<>(ids.size());
        for (int id : ids) {
            BuffTable row = snap.kvData.get(id);
            if (row != null) { result.add(row); }
        }
        return result;
    }

    // ---- RandOne ----

    public BuffTable randOne() {
        Snapshot snap = this.snapshot;
        if (snap.data == null || snap.data.getDataCount() == 0) return null;
        int idx = ThreadLocalRandom.current().nextInt(snap.data.getDataCount());
        return snap.data.getData(idx);
    }

    // ---- Where / First ----

    public List<BuffTable> where(Predicate<BuffTable> pred) {
        Snapshot snap = this.snapshot;
        List<BuffTable> result = new ArrayList<>();
        for (BuffTable row : snap.data.getDataList()) {
            if (pred.test(row)) { result.add(row); }
        }
        return result;
    }

    public BuffTable first(Predicate<BuffTable> pred) {
        Snapshot snap = this.snapshot;
        for (BuffTable row : snap.data.getDataList()) {
            if (pred.test(row)) { return row; }
        }
        return null;
    }
}