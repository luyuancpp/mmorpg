
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
 * Auto-generated config manager for ActorActionState.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public class ActorActionStateTableManager {

    private static final ActorActionStateTableManager INSTANCE = new ActorActionStateTableManager();

    private ActorActionStateTableData data;
    private final Map<Integer, ActorActionStateTable> kvData = new HashMap<>();



    public static ActorActionStateTableManager getInstance() {
        return INSTANCE;
    }

    public void load(String configDir, boolean useBinary) throws Exception {
        ActorActionStateTableData.Builder builder = ActorActionStateTableData.newBuilder();
        if (useBinary) {
            byte[] raw = Files.readAllBytes(Path.of(configDir, "actoractionstate.pb"));
            builder.mergeFrom(raw);
        } else {
            String json = Files.readString(Path.of(configDir, "actoractionstate.json"));
            JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
        }
        this.data = builder.build();

        for (ActorActionStateTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
        }
    }

    public ActorActionStateTableData getAll() {
        return data;
    }

    public ActorActionStateTable getById(int id) {
        return kvData.get(id);
    }

    public Map<Integer, ActorActionStateTable> getKvData() {
        return Collections.unmodifiableMap(kvData);
    }







    // ---- Has / Exists ----

    public boolean hasId(int id) {
        return kvData.containsKey(id);
    }



    // ---- Len / Count ----

    public int size() {
        return kvData.size();
    }




    // ---- Batch Lookup (IN) ----

    public List<ActorActionStateTable> getByIds(List<Integer> ids) {
        List<ActorActionStateTable> result = new ArrayList<>(ids.size());
        for (int id : ids) {
            ActorActionStateTable row = kvData.get(id);
            if (row != null) { result.add(row); }
        }
        return result;
    }

    // ---- Random ----

    public ActorActionStateTable getRandom() {
        if (data == null || data.getDataCount() == 0) return null;
        int idx = ThreadLocalRandom.current().nextInt(data.getDataCount());
        return data.getData(idx);
    }

    // ---- Filter / FindFirst ----

    public List<ActorActionStateTable> filter(Predicate<ActorActionStateTable> pred) {
        List<ActorActionStateTable> result = new ArrayList<>();
        for (ActorActionStateTable row : data.getDataList()) {
            if (pred.test(row)) { result.add(row); }
        }
        return result;
    }

    public ActorActionStateTable findFirst(Predicate<ActorActionStateTable> pred) {
        for (ActorActionStateTable row : data.getDataList()) {
            if (pred.test(row)) { return row; }
        }
        return null;
    }
}