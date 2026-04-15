
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

    /** SELECT * FROM skillpermission */
    public SkillPermissionTableData selectAll() {
        return data;
    }

    /** SELECT COUNT(*) FROM skillpermission */
    public int count() {
        return kvData.size();
    }

    /** SELECT * FROM skillpermission WHERE id = ? */
    public SkillPermissionTable selectById(int id) {
        return kvData.get(id);
    }

    /** SELECT EXISTS(SELECT 1 FROM skillpermission WHERE id = ?) */
    public boolean exists(int id) {
        return kvData.containsKey(id);
    }

    /** SELECT * FROM skillpermission WHERE id IN (?, ?, ...) */
    public List<SkillPermissionTable> selectByIds(List<Integer> ids) {
        List<SkillPermissionTable> result = new ArrayList<>(ids.size());
        for (Integer id : ids) {
            SkillPermissionTable row = kvData.get(id);
            if (row != null) {
                result.add(row);
            }
        }
        return result;
    }

    /** Returns the primary-key map */
    public Map<Integer, SkillPermissionTable> dataMap() {
        return Collections.unmodifiableMap(kvData);
    }




    /** SELECT * FROM skillpermission WHERE ? IN (skill_type) */
    public List<SkillPermissionTable> selectWhereInSkill_type(int key) {
        return idxSkill_type.getOrDefault(key, Collections.emptyList());
    }



}