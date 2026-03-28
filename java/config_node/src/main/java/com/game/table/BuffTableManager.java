
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
 * Auto-generated config manager for Buff.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public class BuffTableManager {

    private static final BuffTableManager INSTANCE = new BuffTableManager();

    private BuffTableData data;
    private final Map<Integer, BuffTable> kvData = new HashMap<>();


    private final Map<Double, List<BuffTable>> idxInterval_effect = new HashMap<>();

    private final Map<Integer, List<BuffTable>> idxSub_buff = new HashMap<>();

    private final Map<Integer, List<BuffTable>> idxTarget_sub_buff = new HashMap<>();


    public static BuffTableManager getInstance() {
        return INSTANCE;
    }

    public void load(String configDir) throws Exception {
        String json = Files.readString(Path.of(configDir, "buff.json"));
        BuffTableData.Builder builder = BuffTableData.newBuilder();
        JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
        this.data = builder.build();

        for (BuffTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
            for (Double elem : row.getIntervalEffectList()) {
                idxInterval_effect.computeIfAbsent(elem, k -> new ArrayList<>()).add(row);
            }
            for (Integer elem : row.getSubBuffList()) {
                idxSub_buff.computeIfAbsent(elem, k -> new ArrayList<>()).add(row);
            }
            for (Integer elem : row.getTargetSubBuffList()) {
                idxTarget_sub_buff.computeIfAbsent(elem, k -> new ArrayList<>()).add(row);
            }
        }
    }

    public BuffTableData getAll() {
        return data;
    }

    public BuffTable getById(int id) {
        return kvData.get(id);
    }

    public Map<Integer, BuffTable> getKvData() {
        return Collections.unmodifiableMap(kvData);
    }




    public List<BuffTable> getByInterval_effectIndex(double key) {
        return idxInterval_effect.getOrDefault(key, Collections.emptyList());
    }

    public List<BuffTable> getBySub_buffIndex(int key) {
        return idxSub_buff.getOrDefault(key, Collections.emptyList());
    }

    public List<BuffTable> getByTarget_sub_buffIndex(int key) {
        return idxTarget_sub_buff.getOrDefault(key, Collections.emptyList());
    }



}