
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
 * Auto-generated config manager for Mirror.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public class MirrorTableManager {

    private static final MirrorTableManager INSTANCE = new MirrorTableManager();

    /**
     * Internal snapshot holding all parsed data and indices.
     * load() builds a new snapshot and swaps it in, replacing the old one.
     */
    private static class Snapshot {
        final MirrorTableData data;
        final Map<Integer, MirrorTable> kvData;




        Snapshot(MirrorTableData data,
                 Map<Integer, MirrorTable> kvData) {
            this.data = data;
            this.kvData = kvData;
        }
    }

    private Snapshot snapshot = new Snapshot(
            MirrorTableData.getDefaultInstance(),
            Collections.emptyMap()
    );

    public static MirrorTableManager getInstance() {
        return INSTANCE;
    }

    public void load(String configDir, boolean useBinary) throws Exception {
        MirrorTableData.Builder builder = MirrorTableData.newBuilder();
        if (useBinary) {
            byte[] raw = Files.readAllBytes(Path.of(configDir, "mirror.pb"));
            builder.mergeFrom(raw);
        } else {
            String json = Files.readString(Path.of(configDir, "mirror.json"));
            JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
        }
        MirrorTableData data = builder.build();

        Map<Integer, MirrorTable> kvData = new HashMap<>(data.getDataCount());

        for (MirrorTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
        }

        this.snapshot = new Snapshot(data, kvData);
    }

    public MirrorTableData findAll() {
        return snapshot.data;
    }

    public MirrorTable findById(int id) {
        return snapshot.kvData.get(id);
    }

    public Map<Integer, MirrorTable> getKvData() {
        return Collections.unmodifiableMap(snapshot.kvData);
    }







    // FK: scene_id → BaseScene.id

    // FK: main_scene_id → World.id


    // ---- Exists ----

    public boolean exists(int id) {
        return snapshot.kvData.containsKey(id);
    }



    // ---- Count ----

    public int count() {
        return snapshot.kvData.size();
    }




    // ---- FindByIds (IN) ----

    public List<MirrorTable> findByIds(List<Integer> ids) {
        Snapshot snap = this.snapshot;
        List<MirrorTable> result = new ArrayList<>(ids.size());
        for (int id : ids) {
            MirrorTable row = snap.kvData.get(id);
            if (row != null) { result.add(row); }
        }
        return result;
    }

    // ---- RandOne ----

    public MirrorTable randOne() {
        Snapshot snap = this.snapshot;
        if (snap.data == null || snap.data.getDataCount() == 0) return null;
        int idx = ThreadLocalRandom.current().nextInt(snap.data.getDataCount());
        return snap.data.getData(idx);
    }

    // ---- Where / First ----

    public List<MirrorTable> where(Predicate<MirrorTable> pred) {
        Snapshot snap = this.snapshot;
        List<MirrorTable> result = new ArrayList<>();
        for (MirrorTable row : snap.data.getDataList()) {
            if (pred.test(row)) { result.add(row); }
        }
        return result;
    }

    public MirrorTable first(Predicate<MirrorTable> pred) {
        Snapshot snap = this.snapshot;
        for (MirrorTable row : snap.data.getDataList()) {
            if (pred.test(row)) { return row; }
        }
        return null;
    }
}