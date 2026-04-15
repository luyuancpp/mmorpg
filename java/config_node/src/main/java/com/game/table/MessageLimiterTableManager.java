
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

    public MessageLimiterTableData findAll() {
        return data;
    }

    public MessageLimiterTable findById(int id) {
        return kvData.get(id);
    }

    public Map<Integer, MessageLimiterTable> getKvData() {
        return Collections.unmodifiableMap(kvData);
    }







    // ---- Exists ----

    public boolean exists(int id) {
        return kvData.containsKey(id);
    }



    // ---- Count ----

    public int count() {
        return kvData.size();
    }




    // ---- FindByIds (IN) ----

    public List<MessageLimiterTable> findByIds(List<Integer> ids) {
        List<MessageLimiterTable> result = new ArrayList<>(ids.size());
        for (int id : ids) {
            MessageLimiterTable row = kvData.get(id);
            if (row != null) { result.add(row); }
        }
        return result;
    }

    // ---- RandOne ----

    public MessageLimiterTable randOne() {
        if (data == null || data.getDataCount() == 0) return null;
        int idx = ThreadLocalRandom.current().nextInt(data.getDataCount());
        return data.getData(idx);
    }

    // ---- Where / First ----

    public List<MessageLimiterTable> where(Predicate<MessageLimiterTable> pred) {
        List<MessageLimiterTable> result = new ArrayList<>();
        for (MessageLimiterTable row : data.getDataList()) {
            if (pred.test(row)) { result.add(row); }
        }
        return result;
    }

    public MessageLimiterTable first(Predicate<MessageLimiterTable> pred) {
        for (MessageLimiterTable row : data.getDataList()) {
            if (pred.test(row)) { return row; }
        }
        return null;
    }
}