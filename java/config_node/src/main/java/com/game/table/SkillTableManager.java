
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
 * Auto-generated config manager for Skill.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public class SkillTableManager {

    private static final SkillTableManager INSTANCE = new SkillTableManager();

    private SkillTableData data;
    private final Map<Integer, SkillTable> kvData = new HashMap<>();


    private final Map<Integer, List<SkillTable>> idxSkill_type = new HashMap<>();

    private final Map<Integer, List<SkillTable>> idxTargeting_mode = new HashMap<>();

    private final Map<Integer, List<SkillTable>> idxEffect = new HashMap<>();


    public static SkillTableManager getInstance() {
        return INSTANCE;
    }

    public void load(String configDir, boolean useBinary) throws Exception {
        SkillTableData.Builder builder = SkillTableData.newBuilder();
        if (useBinary) {
            byte[] raw = Files.readAllBytes(Path.of(configDir, "skill.pb"));
            builder.mergeFrom(raw);
        } else {
            String json = Files.readString(Path.of(configDir, "skill.json"));
            JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
        }
        this.data = builder.build();

        for (SkillTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
            for (Integer elem : row.getSkillTypeList()) {
                idxSkill_type.computeIfAbsent(elem, k -> new ArrayList<>()).add(row);
            }
            for (Integer elem : row.getTargetingModeList()) {
                idxTargeting_mode.computeIfAbsent(elem, k -> new ArrayList<>()).add(row);
            }
            for (Integer elem : row.getEffectList()) {
                idxEffect.computeIfAbsent(elem, k -> new ArrayList<>()).add(row);
            }
        }
    }

    /** SELECT * FROM skill */
    public SkillTableData selectAll() {
        return data;
    }

    /** SELECT COUNT(*) FROM skill */
    public int count() {
        return kvData.size();
    }

    /** SELECT * FROM skill WHERE id = ? */
    public SkillTable selectById(int id) {
        return kvData.get(id);
    }

    /** SELECT EXISTS(SELECT 1 FROM skill WHERE id = ?) */
    public boolean exists(int id) {
        return kvData.containsKey(id);
    }

    /** SELECT * FROM skill WHERE id IN (?, ?, ...) */
    public List<SkillTable> selectByIds(List<Integer> ids) {
        List<SkillTable> result = new ArrayList<>(ids.size());
        for (Integer id : ids) {
            SkillTable row = kvData.get(id);
            if (row != null) {
                result.add(row);
            }
        }
        return result;
    }

    /** Returns the primary-key map */
    public Map<Integer, SkillTable> dataMap() {
        return Collections.unmodifiableMap(kvData);
    }




    /** SELECT * FROM skill WHERE ? IN (skill_type) */
    public List<SkillTable> selectWhereInSkill_type(int key) {
        return idxSkill_type.getOrDefault(key, Collections.emptyList());
    }

    /** SELECT * FROM skill WHERE ? IN (targeting_mode) */
    public List<SkillTable> selectWhereInTargeting_mode(int key) {
        return idxTargeting_mode.getOrDefault(key, Collections.emptyList());
    }

    /** SELECT * FROM skill WHERE ? IN (effect) */
    public List<SkillTable> selectWhereInEffect(int key) {
        return idxEffect.getOrDefault(key, Collections.emptyList());
    }



}