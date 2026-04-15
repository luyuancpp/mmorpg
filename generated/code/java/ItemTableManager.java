
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
 * Auto-generated config manager for Item.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public class ItemTableManager {

    private static final ItemTableManager INSTANCE = new ItemTableManager();

    private ItemTableData data;
    private final Map<Integer, ItemTable> kvData = new HashMap<>();



    public static ItemTableManager getInstance() {
        return INSTANCE;
    }

    public void load(String configDir, boolean useBinary) throws Exception {
        ItemTableData.Builder builder = ItemTableData.newBuilder();
        if (useBinary) {
            byte[] raw = Files.readAllBytes(Path.of(configDir, "item.pb"));
            builder.mergeFrom(raw);
        } else {
            String json = Files.readString(Path.of(configDir, "item.json"));
            JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
        }
        this.data = builder.build();

        for (ItemTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
        }
    }

    public ItemTableData getAll() {
        return data;
    }

    public ItemTable getById(int id) {
        return kvData.get(id);
    }

    public Map<Integer, ItemTable> getKvData() {
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

    public List<ItemTable> getByIds(List<Integer> ids) {
        List<ItemTable> result = new ArrayList<>(ids.size());
        for (int id : ids) {
            ItemTable row = kvData.get(id);
            if (row != null) { result.add(row); }
        }
        return result;
    }

    // ---- Random ----

    public ItemTable getRandom() {
        if (data == null || data.getDataCount() == 0) return null;
        int idx = ThreadLocalRandom.current().nextInt(data.getDataCount());
        return data.getData(idx);
    }

    // ---- Filter / FindFirst ----

    public List<ItemTable> filter(Predicate<ItemTable> pred) {
        List<ItemTable> result = new ArrayList<>();
        for (ItemTable row : data.getDataList()) {
            if (pred.test(row)) { result.add(row); }
        }
        return result;
    }

    public ItemTable findFirst(Predicate<ItemTable> pred) {
        for (ItemTable row : data.getDataList()) {
            if (pred.test(row)) { return row; }
        }
        return null;
    }
}