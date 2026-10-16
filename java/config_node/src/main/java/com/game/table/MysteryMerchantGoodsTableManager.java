
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
 * Auto-generated config manager for MysteryMerchantGoods.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public class MysteryMerchantGoodsTableManager {

    private static final MysteryMerchantGoodsTableManager INSTANCE = new MysteryMerchantGoodsTableManager();

    /**
     * Internal snapshot holding all parsed data and indices.
     * load() builds a new snapshot and swaps it in, replacing the old one.
     */
    private static class Snapshot {
        final MysteryMerchantGoodsTableData data;
        final Map<Integer, MysteryMerchantGoodsTable> kvData;




        final Map<Integer, List<MysteryMerchantGoodsTable>> idxItemId;


        Snapshot(MysteryMerchantGoodsTableData data,
                 Map<Integer, MysteryMerchantGoodsTable> kvData,
                 Map<Integer, List<MysteryMerchantGoodsTable>> idxItemId) {
            this.data = data;
            this.kvData = kvData;
            this.idxItemId = idxItemId;
        }
    }

    private Snapshot snapshot = new Snapshot(
            MysteryMerchantGoodsTableData.getDefaultInstance(),
            Collections.emptyMap(),
            Collections.emptyMap()
    );

    public static MysteryMerchantGoodsTableManager getInstance() {
        return INSTANCE;
    }

    public void load(String configDir, boolean useBinary) throws Exception {
        MysteryMerchantGoodsTableData.Builder builder = MysteryMerchantGoodsTableData.newBuilder();
        if (useBinary) {
            byte[] raw = Files.readAllBytes(Path.of(configDir, "mysterymerchantgoods.pb"));
            builder.mergeFrom(raw);
        } else {
            String json = Files.readString(Path.of(configDir, "mysterymerchantgoods.json"));
            JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
        }
        MysteryMerchantGoodsTableData data = builder.build();

        Map<Integer, MysteryMerchantGoodsTable> kvData = new HashMap<>(data.getDataCount());
        Map<Integer, List<MysteryMerchantGoodsTable>> idxItemId = new HashMap<>();

        for (MysteryMerchantGoodsTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
            idxItemId.computeIfAbsent(row.getItemId(), k -> new ArrayList<>()).add(row);
        }

        this.snapshot = new Snapshot(data, kvData, idxItemId);
    }

    public MysteryMerchantGoodsTableData findAll() {
        return snapshot.data;
    }

    /** 热更契约：返回的对象属于当前快照，调用方**只存 id**，不要长期持有引用。 */
    public MysteryMerchantGoodsTable findById(int id) {
        return snapshot.kvData.get(id);
    }

    public Map<Integer, MysteryMerchantGoodsTable> getKvData() {
        return Collections.unmodifiableMap(snapshot.kvData);
    }







    public List<MysteryMerchantGoodsTable> getByItemId(int key) {
        return snapshot.idxItemId.getOrDefault(key, Collections.emptyList());
    }



    // FK: item_id → Item.id


    // ---- Exists ----

    public boolean exists(int id) {
        return snapshot.kvData.containsKey(id);
    }



    // ---- Count ----

    public int count() {
        return snapshot.kvData.size();
    }




    public int countByItemIdIndex(int key) {
        return snapshot.idxItemId.getOrDefault(key, Collections.emptyList()).size();
    }


    // ---- FindByIds (IN) ----

    public List<MysteryMerchantGoodsTable> findByIds(List<Integer> ids) {
        Snapshot snap = this.snapshot;
        List<MysteryMerchantGoodsTable> result = new ArrayList<>(ids.size());
        for (int id : ids) {
            MysteryMerchantGoodsTable row = snap.kvData.get(id);
            if (row != null) { result.add(row); }
        }
        return result;
    }

    // ---- RandOne ----

    public MysteryMerchantGoodsTable randOne() {
        Snapshot snap = this.snapshot;
        if (snap.data == null || snap.data.getDataCount() == 0) return null;
        int idx = ThreadLocalRandom.current().nextInt(snap.data.getDataCount());
        return snap.data.getData(idx);
    }

    // ---- Where / First ----

    public List<MysteryMerchantGoodsTable> where(Predicate<MysteryMerchantGoodsTable> pred) {
        Snapshot snap = this.snapshot;
        List<MysteryMerchantGoodsTable> result = new ArrayList<>();
        for (MysteryMerchantGoodsTable row : snap.data.getDataList()) {
            if (pred.test(row)) { result.add(row); }
        }
        return result;
    }

    public MysteryMerchantGoodsTable first(Predicate<MysteryMerchantGoodsTable> pred) {
        Snapshot snap = this.snapshot;
        for (MysteryMerchantGoodsTable row : snap.data.getDataList()) {
            if (pred.test(row)) { return row; }
        }
        return null;
    }
}