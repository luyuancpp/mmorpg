
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
 * Auto-generated config manager for Class.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public class ClassTableManager {

    private static final ClassTableManager INSTANCE = new ClassTableManager();

    private ClassTableData data;
    private final Map<Integer, ClassTable> kvData = new HashMap<>();


    private final Map<Integer, List<ClassTable>> idxSkill = new HashMap<>();


    public static ClassTableManager getInstance() {
        return INSTANCE;
    }

    public void load(String configDir, boolean useBinary) throws Exception {
        ClassTableData.Builder builder = ClassTableData.newBuilder();
        if (useBinary) {
            byte[] raw = Files.readAllBytes(Path.of(configDir, "class.pb"));
            builder.mergeFrom(raw);
        } else {
            String json = Files.readString(Path.of(configDir, "class.json"));
            JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
        }
        this.data = builder.build();

        for (ClassTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
            for (Integer elem : row.getSkillList()) {
                idxSkill.computeIfAbsent(elem, k -> new ArrayList<>()).add(row);
            }
        }
    }

    public ClassTableData getAll() {
        return data;
    }

    public ClassTable getById(int id) {
        return kvData.get(id);
    }

    public Map<Integer, ClassTable> getKvData() {
        return Collections.unmodifiableMap(kvData);
    }




    public List<ClassTable> getBySkillIndex(int key) {
        return idxSkill.getOrDefault(key, Collections.emptyList());
    }




    // ---- Has / Exists ----

    public boolean hasId(int id) {
        return kvData.containsKey(id);
    }



    // ---- Len / Count ----

    public int size() {
        return kvData.size();
    }



    public int countBySkillIndex(int key) {
        return idxSkill.getOrDefault(key, Collections.emptyList()).size();
    }


    // ---- Batch Lookup (IN) ----

    public List<ClassTable> getByIds(List<Integer> ids) {
        List<ClassTable> result = new ArrayList<>(ids.size());
        for (int id : ids) {
            ClassTable row = kvData.get(id);
            if (row != null) { result.add(row); }
        }
        return result;
    }

    // ---- Random ----

    public ClassTable getRandom() {
        if (data == null || data.getDataCount() == 0) return null;
        int idx = ThreadLocalRandom.current().nextInt(data.getDataCount());
        return data.getData(idx);
    }

    // ---- Filter / FindFirst ----

    public List<ClassTable> filter(Predicate<ClassTable> pred) {
        List<ClassTable> result = new ArrayList<>();
        for (ClassTable row : data.getDataList()) {
            if (pred.test(row)) { result.add(row); }
        }
        return result;
    }

    public ClassTable findFirst(Predicate<ClassTable> pred) {
        for (ClassTable row : data.getDataList()) {
            if (pred.test(row)) { return row; }
        }
        return null;
    }
}