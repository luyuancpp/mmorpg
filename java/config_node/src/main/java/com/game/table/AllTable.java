
package com.game.table;

import java.util.concurrent.CountDownLatch;

/**
 * Auto-generated aggregated table loader.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public class AllTable {

    private static Runnable loadSuccessCallback;

    /**
     * Load all config tables synchronously.
     * @param useBinary true to load .pb (proto binary), false to load .json.
     */
    public static void loadTables(String configDir, boolean useBinary) throws Exception {

        ActorActionCombatStateTableManager.getInstance().load(configDir, useBinary);

        ActorActionStateTableManager.getInstance().load(configDir, useBinary);

        BuffTableManager.getInstance().load(configDir, useBinary);

        ClassTableManager.getInstance().load(configDir, useBinary);

        ConditionTableManager.getInstance().load(configDir, useBinary);

        CooldownTableManager.getInstance().load(configDir, useBinary);

        GlobalVariableTableManager.getInstance().load(configDir, useBinary);

        ItemTableManager.getInstance().load(configDir, useBinary);

        MainSceneTableManager.getInstance().load(configDir, useBinary);

        MessageLimiterTableManager.getInstance().load(configDir, useBinary);

        MissionTableManager.getInstance().load(configDir, useBinary);

        MonsterTableManager.getInstance().load(configDir, useBinary);

        RewardTableManager.getInstance().load(configDir, useBinary);

        SceneTableManager.getInstance().load(configDir, useBinary);

        SkillTableManager.getInstance().load(configDir, useBinary);

        SkillPermissionTableManager.getInstance().load(configDir, useBinary);

        TestTableManager.getInstance().load(configDir, useBinary);

        TestMultiKeyTableManager.getInstance().load(configDir, useBinary);

        if (loadSuccessCallback != null) {
            loadSuccessCallback.run();
        }
    }

    /** Backward-compatible overload — defaults to JSON. */
    public static void loadTables(String configDir) throws Exception {
        loadTables(configDir, false);
    }

    /**
     * Load all config tables asynchronously using threads.
     * @param useBinary true to load .pb (proto binary), false to load .json.
     */
    public static void loadTablesAsync(String configDir, boolean useBinary) throws Exception {
        CountDownLatch latch = new CountDownLatch(18);

        new Thread(() -> {
            try {
                ActorActionCombatStateTableManager.getInstance().load(configDir, useBinary);
            } catch (Exception e) {
                throw new RuntimeException("Failed to load ActorActionCombatState table", e);
            } finally {
                latch.countDown();
            }
        }).start();

        new Thread(() -> {
            try {
                ActorActionStateTableManager.getInstance().load(configDir, useBinary);
            } catch (Exception e) {
                throw new RuntimeException("Failed to load ActorActionState table", e);
            } finally {
                latch.countDown();
            }
        }).start();

        new Thread(() -> {
            try {
                BuffTableManager.getInstance().load(configDir, useBinary);
            } catch (Exception e) {
                throw new RuntimeException("Failed to load Buff table", e);
            } finally {
                latch.countDown();
            }
        }).start();

        new Thread(() -> {
            try {
                ClassTableManager.getInstance().load(configDir, useBinary);
            } catch (Exception e) {
                throw new RuntimeException("Failed to load Class table", e);
            } finally {
                latch.countDown();
            }
        }).start();

        new Thread(() -> {
            try {
                ConditionTableManager.getInstance().load(configDir, useBinary);
            } catch (Exception e) {
                throw new RuntimeException("Failed to load Condition table", e);
            } finally {
                latch.countDown();
            }
        }).start();

        new Thread(() -> {
            try {
                CooldownTableManager.getInstance().load(configDir, useBinary);
            } catch (Exception e) {
                throw new RuntimeException("Failed to load Cooldown table", e);
            } finally {
                latch.countDown();
            }
        }).start();

        new Thread(() -> {
            try {
                GlobalVariableTableManager.getInstance().load(configDir, useBinary);
            } catch (Exception e) {
                throw new RuntimeException("Failed to load GlobalVariable table", e);
            } finally {
                latch.countDown();
            }
        }).start();

        new Thread(() -> {
            try {
                ItemTableManager.getInstance().load(configDir, useBinary);
            } catch (Exception e) {
                throw new RuntimeException("Failed to load Item table", e);
            } finally {
                latch.countDown();
            }
        }).start();

        new Thread(() -> {
            try {
                MainSceneTableManager.getInstance().load(configDir, useBinary);
            } catch (Exception e) {
                throw new RuntimeException("Failed to load MainScene table", e);
            } finally {
                latch.countDown();
            }
        }).start();

        new Thread(() -> {
            try {
                MessageLimiterTableManager.getInstance().load(configDir, useBinary);
            } catch (Exception e) {
                throw new RuntimeException("Failed to load MessageLimiter table", e);
            } finally {
                latch.countDown();
            }
        }).start();

        new Thread(() -> {
            try {
                MissionTableManager.getInstance().load(configDir, useBinary);
            } catch (Exception e) {
                throw new RuntimeException("Failed to load Mission table", e);
            } finally {
                latch.countDown();
            }
        }).start();

        new Thread(() -> {
            try {
                MonsterTableManager.getInstance().load(configDir, useBinary);
            } catch (Exception e) {
                throw new RuntimeException("Failed to load Monster table", e);
            } finally {
                latch.countDown();
            }
        }).start();

        new Thread(() -> {
            try {
                RewardTableManager.getInstance().load(configDir, useBinary);
            } catch (Exception e) {
                throw new RuntimeException("Failed to load Reward table", e);
            } finally {
                latch.countDown();
            }
        }).start();

        new Thread(() -> {
            try {
                SceneTableManager.getInstance().load(configDir, useBinary);
            } catch (Exception e) {
                throw new RuntimeException("Failed to load Scene table", e);
            } finally {
                latch.countDown();
            }
        }).start();

        new Thread(() -> {
            try {
                SkillTableManager.getInstance().load(configDir, useBinary);
            } catch (Exception e) {
                throw new RuntimeException("Failed to load Skill table", e);
            } finally {
                latch.countDown();
            }
        }).start();

        new Thread(() -> {
            try {
                SkillPermissionTableManager.getInstance().load(configDir, useBinary);
            } catch (Exception e) {
                throw new RuntimeException("Failed to load SkillPermission table", e);
            } finally {
                latch.countDown();
            }
        }).start();

        new Thread(() -> {
            try {
                TestTableManager.getInstance().load(configDir, useBinary);
            } catch (Exception e) {
                throw new RuntimeException("Failed to load Test table", e);
            } finally {
                latch.countDown();
            }
        }).start();

        new Thread(() -> {
            try {
                TestMultiKeyTableManager.getInstance().load(configDir, useBinary);
            } catch (Exception e) {
                throw new RuntimeException("Failed to load TestMultiKey table", e);
            } finally {
                latch.countDown();
            }
        }).start();

        latch.await();

        if (loadSuccessCallback != null) {
            loadSuccessCallback.run();
        }
    }

    /**
     * Register a callback invoked after all tables load.
     */
    public static void onTablesLoadSuccess(Runnable callback) {
        loadSuccessCallback = callback;
    }

    /**
     * Reload all config tables (hot-reload safe).
     * @param useBinary true to load .pb (proto binary), false to load .json.
     */
    public static void reloadTables(String configDir, boolean useBinary) throws Exception {

        ActorActionCombatStateTableManager.getInstance().load(configDir, useBinary);

        ActorActionStateTableManager.getInstance().load(configDir, useBinary);

        BuffTableManager.getInstance().load(configDir, useBinary);

        ClassTableManager.getInstance().load(configDir, useBinary);

        ConditionTableManager.getInstance().load(configDir, useBinary);

        CooldownTableManager.getInstance().load(configDir, useBinary);

        GlobalVariableTableManager.getInstance().load(configDir, useBinary);

        ItemTableManager.getInstance().load(configDir, useBinary);

        MainSceneTableManager.getInstance().load(configDir, useBinary);

        MessageLimiterTableManager.getInstance().load(configDir, useBinary);

        MissionTableManager.getInstance().load(configDir, useBinary);

        MonsterTableManager.getInstance().load(configDir, useBinary);

        RewardTableManager.getInstance().load(configDir, useBinary);

        SceneTableManager.getInstance().load(configDir, useBinary);

        SkillTableManager.getInstance().load(configDir, useBinary);

        SkillPermissionTableManager.getInstance().load(configDir, useBinary);

        TestTableManager.getInstance().load(configDir, useBinary);

        TestMultiKeyTableManager.getInstance().load(configDir, useBinary);

        if (loadSuccessCallback != null) {
            loadSuccessCallback.run();
        }
    }

    /** Backward-compatible overload — defaults to JSON. */
    public static void reloadTables(String configDir) throws Exception {
        reloadTables(configDir, false);
    }
}