
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

    public void load(String configDir) throws Exception {
        String json = Files.readString(Path.of(configDir, "skill.json"));
        SkillTableData.Builder builder = SkillTableData.newBuilder();
        JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
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

    public SkillTableData getAll() {
        return data;
    }

    public SkillTable getById(int id) {
        return kvData.get(id);
    }

    public Map<Integer, SkillTable> getKvData() {
        return Collections.unmodifiableMap(kvData);
    }




    public List<SkillTable> getBySkill_typeIndex(int key) {
        return idxSkill_type.getOrDefault(key, Collections.emptyList());
    }

    public List<SkillTable> getByTargeting_modeIndex(int key) {
        return idxTargeting_mode.getOrDefault(key, Collections.emptyList());
    }

    public List<SkillTable> getByEffectIndex(int key) {
        return idxEffect.getOrDefault(key, Collections.emptyList());
    }



}