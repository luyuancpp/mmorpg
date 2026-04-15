
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







    // ---- Has / Exists ----

    public boolean hasId(int id) {
        return kvData.containsKey(id);
    }



    // ---- Len / Count ----

    public int size() {
        return kvData.size();
    }




    // ---- Batch Lookup (IN) ----

    public List<MessageLimiterTable> getByIds(List<Integer> ids) {
        List<MessageLimiterTable> result = new ArrayList<>(ids.size());
        for (int id : ids) {
            MessageLimiterTable row = kvData.get(id);
            if (row != null) { result.add(row); }
        }
        return result;
    }

    // ---- Random ----

    public MessageLimiterTable getRandom() {
        if (data == null || data.getDataCount() == 0) return null;
        int idx = ThreadLocalRandom.current().nextInt(data.getDataCount());
        return data.getData(idx);
    }

    // ---- Filter / FindFirst ----

    public List<MessageLimiterTable> filter(Predicate<MessageLimiterTable> pred) {
        List<MessageLimiterTable> result = new ArrayList<>();
        for (MessageLimiterTable row : data.getDataList()) {
            if (pred.test(row)) { result.add(row); }
        }
        return result;
    }

    public MessageLimiterTable findFirst(Predicate<MessageLimiterTable> pred) {
        for (MessageLimiterTable row : data.getDataList()) {
            if (pred.test(row)) { return row; }
        }
        return null;
    }
}