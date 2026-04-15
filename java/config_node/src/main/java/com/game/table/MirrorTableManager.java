
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
 * Auto-generated config manager for Mirror.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public class MirrorTableManager {

    private static final MirrorTableManager INSTANCE = new MirrorTableManager();

    private MirrorTableData data;
    private final Map<Integer, MirrorTable> kvData = new HashMap<>();



    public static MirrorTableManager getInstance() {
        return INSTANCE;
    }

    public void load(String configDir, boolean useBinary) throws Exception {
        MirrorTableData.Builder builder = MirrorTableData.newBuilder();
        if (useBinary) {
            byte[] raw = Files.readAllBytes(Path.of(configDir, "mirror.pb"));
            builder.mergeFrom(raw);
        } else {
            String json = Files.readString(Path.of(configDir, "mirror.json"));
            JsonFormat.parser().ignoringUnknownFields().merge(json, builder);
        }
        this.data = builder.build();

        for (MirrorTable row : data.getDataList()) {
            kvData.put(row.getId(), row);
        }
    }

    /** SELECT * FROM mirror */
    public MirrorTableData selectAll() {
        return data;
    }

    /** SELECT COUNT(*) FROM mirror */
    public int count() {
        return kvData.size();
    }

    /** SELECT * FROM mirror WHERE id = ? */
    public MirrorTable selectById(int id) {
        return kvData.get(id);
    }

    /** SELECT EXISTS(SELECT 1 FROM mirror WHERE id = ?) */
    public boolean exists(int id) {
        return kvData.containsKey(id);
    }

    /** SELECT * FROM mirror WHERE id IN (?, ?, ...) */
    public List<MirrorTable> selectByIds(List<Integer> ids) {
        List<MirrorTable> result = new ArrayList<>(ids.size());
        for (Integer id : ids) {
            MirrorTable row = kvData.get(id);
            if (row != null) {
                result.add(row);
            }
        }
        return result;
    }

    /** Returns the primary-key map */
    public Map<Integer, MirrorTable> dataMap() {
        return Collections.unmodifiableMap(kvData);
    }






    // FK: scene_id -> BaseScene.id

    // FK: main_scene_id -> World.id

}