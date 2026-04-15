
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

    /** SELECT * FROM item */
    public ItemTableData selectAll() {
        return data;
    }

    /** SELECT COUNT(*) FROM item */
    public int count() {
        return kvData.size();
    }

    /** SELECT * FROM item WHERE id = ? */
    public ItemTable selectById(int id) {
        return kvData.get(id);
    }

    /** SELECT EXISTS(SELECT 1 FROM item WHERE id = ?) */
    public boolean exists(int id) {
        return kvData.containsKey(id);
    }

    /** SELECT * FROM item WHERE id IN (?, ?, ...) */
    public List<ItemTable> selectByIds(List<Integer> ids) {
        List<ItemTable> result = new ArrayList<>(ids.size());
        for (Integer id : ids) {
            ItemTable row = kvData.get(id);
            if (row != null) {
                result.add(row);
            }
        }
        return result;
    }

    /** Returns the primary-key map */
    public Map<Integer, ItemTable> dataMap() {
        return Collections.unmodifiableMap(kvData);
    }






}