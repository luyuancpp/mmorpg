
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

    public SkillTableData findAll() {
        return data;
    }

    public SkillTable findById(int id) {
        return kvData.get(id);
    }

    public Map<Integer, SkillTable> getKvData() {
        return Collections.unmodifiableMap(kvData);
    }




    public List<SkillTable> findBySkill_typeIndex(int key) {
        return idxSkill_type.getOrDefault(key, Collections.emptyList());
    }

    public List<SkillTable> findByTargeting_modeIndex(int key) {
        return idxTargeting_mode.getOrDefault(key, Collections.emptyList());
    }

    public List<SkillTable> findByEffectIndex(int key) {
        return idxEffect.getOrDefault(key, Collections.emptyList());
    }




    // ---- Exists ----

    public boolean exists(int id) {
        return kvData.containsKey(id);
    }



    // ---- Count ----

    public int count() {
        return kvData.size();
    }



    public int countBySkill_typeIndex(int key) {
        return idxSkill_type.getOrDefault(key, Collections.emptyList()).size();
    }

    public int countByTargeting_modeIndex(int key) {
        return idxTargeting_mode.getOrDefault(key, Collections.emptyList()).size();
    }

    public int countByEffectIndex(int key) {
        return idxEffect.getOrDefault(key, Collections.emptyList()).size();
    }


    // ---- FindByIds (IN) ----

    public List<SkillTable> findByIds(List<Integer> ids) {
        List<SkillTable> result = new ArrayList<>(ids.size());
        for (int id : ids) {
            SkillTable row = kvData.get(id);
            if (row != null) { result.add(row); }
        }
        return result;
    }

    // ---- RandOne ----

    public SkillTable randOne() {
        if (data == null || data.getDataCount() == 0) return null;
        int idx = ThreadLocalRandom.current().nextInt(data.getDataCount());
        return data.getData(idx);
    }

    // ---- Where / First ----

    public List<SkillTable> where(Predicate<SkillTable> pred) {
        List<SkillTable> result = new ArrayList<>();
        for (SkillTable row : data.getDataList()) {
            if (pred.test(row)) { result.add(row); }
        }
        return result;
    }

    public SkillTable first(Predicate<SkillTable> pred) {
        for (SkillTable row : data.getDataList()) {
            if (pred.test(row)) { return row; }
        }
        return null;
    }
}