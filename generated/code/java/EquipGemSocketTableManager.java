
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
 * Auto-generated config manager for EquipGemSocket.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public class EquipGemSocketTableManager {

    private static final EquipGemSocketTableManager INSTANCE = new EquipGemSocketTableManager();

    /**
     * Internal snapshot holding all parsed data and indices.
     * load() builds a new snapshot and swaps it in, replacing the old one.
     */
    private static class Snapshot {
        final EquipGemSocketTableData data;
        final Map<Integer, EquipGemSocketTable> kvData;




        final Map<Integer, List<EquipGemSocketTable>> idxEquipKind;


        Snapshot(EquipGemSocketTableData data,
                 Map<Integer, EquipGemSocketTable> kvData,
                 Map<Integer, List<EquipGemSocketTable>> idxEquipKind) {
            this.data = data;
            this.kvData = kvData;
            this.idxEquipKind = idxEquipKind;
        }
    }

    private Snapshot snapshot = new Snapshot(
            EquipGemSocketTableData.getDefaultInstance(),
            Collections.emptyMap(),
            Collections.emptyMap()
    );

    public static EquipGemSocketTableManager getInstance() {
        return INSTANCE;
    }

    public void load(String configDir, boolean useBinary) throws Exception {
        EquipGemSocketTableData.Builder builder = EquipGemSocketTableData.newBuilder();
        if (useBinary) {
            byte[] raw = Files.readAllBytes(Path.of(configDir, "equipgemsocket.pb"));
            builder.mergeFrom(raw);
        } else {
            String json = Files.readString(Path.of(configDir, "equipgemsocket.json"));
            JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
        }
        EquipGemSocketTableData data = builder.build();

        Map<Integer, EquipGemSocketTable> kvData = new HashMap<>(data.getDataCount());
        Map<Integer, List<EquipGemSocketTable>> idxEquipKind = new HashMap<>();

        for (EquipGemSocketTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
            idxEquipKind.computeIfAbsent(row.getEquipKind(), k -> new ArrayList<>()).add(row);
        }

        this.snapshot = new Snapshot(data, kvData, idxEquipKind);
    }

    public EquipGemSocketTableData findAll() {
        return snapshot.data;
    }

    /** 热更契约：返回的对象属于当前快照，调用方**只存 id**，不要长期持有引用。 */
    public EquipGemSocketTable findById(int id) {
        return snapshot.kvData.get(id);
    }

    public Map<Integer, EquipGemSocketTable> getKvData() {
        return Collections.unmodifiableMap(snapshot.kvData);
    }







    public List<EquipGemSocketTable> getByEquipKind(int key) {
        return snapshot.idxEquipKind.getOrDefault(key, Collections.emptyList());
    }




    // ---- Exists ----

    public boolean exists(int id) {
        return snapshot.kvData.containsKey(id);
    }



    // ---- Count ----

    public int count() {
        return snapshot.kvData.size();
    }




    public int countByEquipKindIndex(int key) {
        return snapshot.idxEquipKind.getOrDefault(key, Collections.emptyList()).size();
    }


    // ---- FindByIds (IN) ----

    public List<EquipGemSocketTable> findByIds(List<Integer> ids) {
        Snapshot snap = this.snapshot;
        List<EquipGemSocketTable> result = new ArrayList<>(ids.size());
        for (int id : ids) {
            EquipGemSocketTable row = snap.kvData.get(id);
            if (row != null) { result.add(row); }
        }
        return result;
    }

    // ---- RandOne ----

    public EquipGemSocketTable randOne() {
        Snapshot snap = this.snapshot;
        if (snap.data == null || snap.data.getDataCount() == 0) return null;
        int idx = ThreadLocalRandom.current().nextInt(snap.data.getDataCount());
        return snap.data.getData(idx);
    }

    // ---- Where / First ----

    public List<EquipGemSocketTable> where(Predicate<EquipGemSocketTable> pred) {
        Snapshot snap = this.snapshot;
        List<EquipGemSocketTable> result = new ArrayList<>();
        for (EquipGemSocketTable row : snap.data.getDataList()) {
            if (pred.test(row)) { result.add(row); }
        }
        return result;
    }

    public EquipGemSocketTable first(Predicate<EquipGemSocketTable> pred) {
        Snapshot snap = this.snapshot;
        for (EquipGemSocketTable row : snap.data.getDataList()) {
            if (pred.test(row)) { return row; }
        }
        return null;
    }
}