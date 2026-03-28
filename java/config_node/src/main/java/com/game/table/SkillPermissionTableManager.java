
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

    public void load(String configDir) throws Exception {
        String json = Files.readString(Path.of(configDir, "skillpermission.json"));
        SkillPermissionTableData.Builder builder = SkillPermissionTableData.newBuilder();
        JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
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



}