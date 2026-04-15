
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
 * Auto-generated config manager for SkillPermission.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public class SkillPermissionTableManager {

    private static final SkillPermissionTableManager INSTANCE = new SkillPermissionTableManager();

    private SkillPermissionTableData data;
    private final Map<Integer, SkillPermissionTable> kvData = new HashMap<>();


    private final Map<Integer, List<SkillPermissionTable>> idxSkill_type = new HashMap<>();


    public static SkillPermissionTableManager getInstance() {
        return INSTANCE;
    }

    public void load(String configDir, boolean useBinary) throws Exception {
        SkillPermissionTableData.Builder builder = SkillPermissionTableData.newBuilder();
        if (useBinary) {
            byte[] raw = Files.readAllBytes(Path.of(configDir, "skillpermission.pb"));
            builder.mergeFrom(raw);
        } else {
            String json = Files.readString(Path.of(configDir, "skillpermission.json"));
            JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
        }
        this.data = builder.build();

        for (SkillPermissionTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
            for (Integer elem : row.getSkillTypeList()) {
                idxSkill_type.computeIfAbsent(elem, k -> new ArrayList<>()).add(row);
            }
        }
    }

    public SkillPermissionTableData getAll() {
        return data;
    }

    public SkillPermissionTable getById(int id) {
        return kvData.get(id);
    }

    public Map<Integer, SkillPermissionTable> getKvData() {
        return Collections.unmodifiableMap(kvData);
    }




    public List<SkillPermissionTable> getBySkill_typeIndex(int key) {
        return idxSkill_type.getOrDefault(key, Collections.emptyList());
    }




    // ---- Has / Exists ----

    public boolean hasId(int id) {
        return kvData.containsKey(id);
    }



    // ---- Len / Count ----

    public int size() {
        return kvData.size();
    }



    public int countBySkill_typeIndex(int key) {
        return idxSkill_type.getOrDefault(key, Collections.emptyList()).size();
    }


    // ---- Batch Lookup (IN) ----

    public List<SkillPermissionTable> getByIds(List<Integer> ids) {
        List<SkillPermissionTable> result = new ArrayList<>(ids.size());
        for (int id : ids) {
            SkillPermissionTable row = kvData.get(id);
            if (row != null) { result.add(row); }
        }
        return result;
    }

    // ---- Random ----

    public SkillPermissionTable getRandom() {
        if (data == null || data.getDataCount() == 0) return null;
        int idx = ThreadLocalRandom.current().nextInt(data.getDataCount());
        return data.getData(idx);
    }

    // ---- Filter / FindFirst ----

    public List<SkillPermissionTable> filter(Predicate<SkillPermissionTable> pred) {
        List<SkillPermissionTable> result = new ArrayList<>();
        for (SkillPermissionTable row : data.getDataList()) {
            if (pred.test(row)) { result.add(row); }
        }
        return result;
    }

    public SkillPermissionTable findFirst(Predicate<SkillPermissionTable> pred) {
        for (SkillPermissionTable row : data.getDataList()) {
            if (pred.test(row)) { return row; }
        }
        return null;
    }
}