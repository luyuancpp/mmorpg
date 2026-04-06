
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
 * Auto-generated config manager for MessageLimiter.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public class MessageLimiterTableManager {

    private static final MessageLimiterTableManager INSTANCE = new MessageLimiterTableManager();

    private MessageLimiterTableData data;
    private final Map<Integer, MessageLimiterTable> kvData = new HashMap<>();



    public static MessageLimiterTableManager getInstance() {
        return INSTANCE;
    }

    public void load(String configDir, boolean useBinary) throws Exception {
        MessageLimiterTableData.Builder builder = MessageLimiterTableData.newBuilder();
        if (useBinary) {
            byte[] raw = Files.readAllBytes(Path.of(configDir, "messagelimiter.pb"));
            builder.mergeFrom(raw);
        } else {
            String json = Files.readString(Path.of(configDir, "messagelimiter.json"));
            JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
        }
        this.data = builder.build();

        for (MessageLimiterTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
        }
    }

    public MessageLimiterTableData getAll() {
        return data;
    }

    public MessageLimiterTable getById(int id) {
        return kvData.get(id);
    }

    public Map<Integer, MessageLimiterTable> getKvData() {
        return Collections.unmodifiableMap(kvData);
    }






}