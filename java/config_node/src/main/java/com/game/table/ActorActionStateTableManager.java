
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

    public void load(String configDir) throws Exception {
        String json = Files.readString(Path.of(configDir, "actoractionstate.json"));
        ActorActionStateTableData.Builder builder = ActorActionStateTableData.newBuilder();
        JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
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






}