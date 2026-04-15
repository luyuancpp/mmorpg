
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
 * Auto-generated config manager for ActorActionCombatState.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public class ActorActionCombatStateTableManager {

    private static final ActorActionCombatStateTableManager INSTANCE = new ActorActionCombatStateTableManager();

    private ActorActionCombatStateTableData data;
    private final Map<Integer, ActorActionCombatStateTable> kvData = new HashMap<>();



    public static ActorActionCombatStateTableManager getInstance() {
        return INSTANCE;
    }

    public void load(String configDir, boolean useBinary) throws Exception {
        ActorActionCombatStateTableData.Builder builder = ActorActionCombatStateTableData.newBuilder();
        if (useBinary) {
            byte[] raw = Files.readAllBytes(Path.of(configDir, "actoractioncombatstate.pb"));
            builder.mergeFrom(raw);
        } else {
            String json = Files.readString(Path.of(configDir, "actoractioncombatstate.json"));
            JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
        }
        this.data = builder.build();

        for (ActorActionCombatStateTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
        }
    }

    public ActorActionCombatStateTableData getAll() {
        return data;
    }

    public ActorActionCombatStateTable getById(int id) {
        return kvData.get(id);
    }

    public Map<Integer, ActorActionCombatStateTable> getKvData() {
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

    public List<ActorActionCombatStateTable> getByIds(List<Integer> ids) {
        List<ActorActionCombatStateTable> result = new ArrayList<>(ids.size());
        for (int id : ids) {
            ActorActionCombatStateTable row = kvData.get(id);
            if (row != null) { result.add(row); }
        }
        return result;
    }

    // ---- Random ----

    public ActorActionCombatStateTable getRandom() {
        if (data == null || data.getDataCount() == 0) return null;
        int idx = ThreadLocalRandom.current().nextInt(data.getDataCount());
        return data.getData(idx);
    }

    // ---- Filter / FindFirst ----

    public List<ActorActionCombatStateTable> filter(Predicate<ActorActionCombatStateTable> pred) {
        List<ActorActionCombatStateTable> result = new ArrayList<>();
        for (ActorActionCombatStateTable row : data.getDataList()) {
            if (pred.test(row)) { result.add(row); }
        }
        return result;
    }

    public ActorActionCombatStateTable findFirst(Predicate<ActorActionCombatStateTable> pred) {
        for (ActorActionCombatStateTable row : data.getDataList()) {
            if (pred.test(row)) { return row; }
        }
        return null;
    }
}