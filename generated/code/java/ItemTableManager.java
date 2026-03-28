
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

    public void load(String configDir) throws Exception {
        String json = Files.readString(Path.of(configDir, "item.json"));
        ItemTableData.Builder builder = ItemTableData.newBuilder();
        JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
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






}