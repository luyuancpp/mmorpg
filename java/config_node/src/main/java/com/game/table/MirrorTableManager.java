
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

    private MirrorTableData data;
    private final Map<Integer, MirrorTable> kvData = new HashMap<>();



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
        this.data = builder.build();

        for (MirrorTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
        }
    }

    public MirrorTableData getAll() {
        return data;
    }

    public MirrorTable getById(int id) {
        return kvData.get(id);
    }

    public Map<Integer, MirrorTable> getKvData() {
        return Collections.unmodifiableMap(kvData);
    }






    // FK: scene_id → BaseScene.id

    // FK: main_scene_id → World.id


    // ---- Has / Exists ----

    public boolean hasId(int id) {
        return kvData.containsKey(id);
    }



    // ---- Len / Count ----

    public int size() {
        return kvData.size();
    }




    // ---- Batch Lookup (IN) ----

    public List<MirrorTable> getByIds(List<Integer> ids) {
        List<MirrorTable> result = new ArrayList<>(ids.size());
        for (int id : ids) {
            MirrorTable row = kvData.get(id);
            if (row != null) { result.add(row); }
        }
        return result;
    }

    // ---- Random ----

    public MirrorTable getRandom() {
        if (data == null || data.getDataCount() == 0) return null;
        int idx = ThreadLocalRandom.current().nextInt(data.getDataCount());
        return data.getData(idx);
    }

    // ---- Filter / FindFirst ----

    public List<MirrorTable> filter(Predicate<MirrorTable> pred) {
        List<MirrorTable> result = new ArrayList<>();
        for (MirrorTable row : data.getDataList()) {
            if (pred.test(row)) { result.add(row); }
        }
        return result;
    }

    public MirrorTable findFirst(Predicate<MirrorTable> pred) {
        for (MirrorTable row : data.getDataList()) {
            if (pred.test(row)) { return row; }
        }
        return null;
    }
}