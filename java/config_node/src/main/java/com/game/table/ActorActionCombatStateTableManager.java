
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

    public void load(String configDir) throws Exception {
        String json = Files.readString(Path.of(configDir, "actoractioncombatstate.json"));
        ActorActionCombatStateTableData.Builder builder = ActorActionCombatStateTableData.newBuilder();
        JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
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






}