
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
     */
    public static void loadTables(String configDir) throws Exception {

        ActorActionCombatStateTableManager.getInstance().load(configDir);

        ActorActionStateTableManager.getInstance().load(configDir);

        BuffTableManager.getInstance().load(configDir);

        ClassTableManager.getInstance().load(configDir);

        ConditionTableManager.getInstance().load(configDir);

        CooldownTableManager.getInstance().load(configDir);

        GlobalVariableTableManager.getInstance().load(configDir);

        ItemTableManager.getInstance().load(configDir);

        MainSceneTableManager.getInstance().load(configDir);

        MessageLimiterTableManager.getInstance().load(configDir);

        MissionTableManager.getInstance().load(configDir);

        MonsterTableManager.getInstance().load(configDir);

        RewardTableManager.getInstance().load(configDir);

        SceneTableManager.getInstance().load(configDir);

        SkillTableManager.getInstance().load(configDir);

        SkillPermissionTableManager.getInstance().load(configDir);

        TestTableManager.getInstance().load(configDir);

        TestMultiKeyTableManager.getInstance().load(configDir);

        if (loadSuccessCallback != null) {
            loadSuccessCallback.run();
        }
    }

    /**
     * Load all config tables asynchronously using threads.
     */
    public static void loadTablesAsync(String configDir) throws Exception {
        CountDownLatch latch = new CountDownLatch(18);

        new Thread(() -> {
            try {
                ActorActionCombatStateTableManager.getInstance().load(configDir);
            } catch (Exception e) {
                throw new RuntimeException("Failed to load ActorActionCombatState table", e);
            } finally {
                latch.countDown();
            }
        }).start();

        new Thread(() -> {
            try {
                ActorActionStateTableManager.getInstance().load(configDir);
            } catch (Exception e) {
                throw new RuntimeException("Failed to load ActorActionState table", e);
            } finally {
                latch.countDown();
            }
        }).start();

        new Thread(() -> {
            try {
                BuffTableManager.getInstance().load(configDir);
            } catch (Exception e) {
                throw new RuntimeException("Failed to load Buff table", e);
            } finally {
                latch.countDown();
            }
        }).start();

        new Thread(() -> {
            try {
                ClassTableManager.getInstance().load(configDir);
            } catch (Exception e) {
                throw new RuntimeException("Failed to load Class table", e);
            } finally {
                latch.countDown();
            }
        }).start();

        new Thread(() -> {
            try {
                ConditionTableManager.getInstance().load(configDir);
            } catch (Exception e) {
                throw new RuntimeException("Failed to load Condition table", e);
            } finally {
                latch.countDown();
            }
        }).start();

        new Thread(() -> {
            try {
                CooldownTableManager.getInstance().load(configDir);
            } catch (Exception e) {
                throw new RuntimeException("Failed to load Cooldown table", e);
            } finally {
                latch.countDown();
            }
        }).start();

        new Thread(() -> {
            try {
                GlobalVariableTableManager.getInstance().load(configDir);
            } catch (Exception e) {
                throw new RuntimeException("Failed to load GlobalVariable table", e);
            } finally {
                latch.countDown();
            }
        }).start();

        new Thread(() -> {
            try {
                ItemTableManager.getInstance().load(configDir);
            } catch (Exception e) {
                throw new RuntimeException("Failed to load Item table", e);
            } finally {
                latch.countDown();
            }
        }).start();

        new Thread(() -> {
            try {
                MainSceneTableManager.getInstance().load(configDir);
            } catch (Exception e) {
                throw new RuntimeException("Failed to load MainScene table", e);
            } finally {
                latch.countDown();
            }
        }).start();

        new Thread(() -> {
            try {
                MessageLimiterTableManager.getInstance().load(configDir);
            } catch (Exception e) {
                throw new RuntimeException("Failed to load MessageLimiter table", e);
            } finally {
                latch.countDown();
            }
        }).start();

        new Thread(() -> {
            try {
                MissionTableManager.getInstance().load(configDir);
            } catch (Exception e) {
                throw new RuntimeException("Failed to load Mission table", e);
            } finally {
                latch.countDown();
            }
        }).start();

        new Thread(() -> {
            try {
                MonsterTableManager.getInstance().load(configDir);
            } catch (Exception e) {
                throw new RuntimeException("Failed to load Monster table", e);
            } finally {
                latch.countDown();
            }
        }).start();

        new Thread(() -> {
            try {
                RewardTableManager.getInstance().load(configDir);
            } catch (Exception e) {
                throw new RuntimeException("Failed to load Reward table", e);
            } finally {
                latch.countDown();
            }
        }).start();

        new Thread(() -> {
            try {
                SceneTableManager.getInstance().load(configDir);
            } catch (Exception e) {
                throw new RuntimeException("Failed to load Scene table", e);
            } finally {
                latch.countDown();
            }
        }).start();

        new Thread(() -> {
            try {
                SkillTableManager.getInstance().load(configDir);
            } catch (Exception e) {
                throw new RuntimeException("Failed to load Skill table", e);
            } finally {
                latch.countDown();
            }
        }).start();

        new Thread(() -> {
            try {
                SkillPermissionTableManager.getInstance().load(configDir);
            } catch (Exception e) {
                throw new RuntimeException("Failed to load SkillPermission table", e);
            } finally {
                latch.countDown();
            }
        }).start();

        new Thread(() -> {
            try {
                TestTableManager.getInstance().load(configDir);
            } catch (Exception e) {
                throw new RuntimeException("Failed to load Test table", e);
            } finally {
                latch.countDown();
            }
        }).start();

        new Thread(() -> {
            try {
                TestMultiKeyTableManager.getInstance().load(configDir);
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
     * Each table manager's load() replaces its internal data atomically.
     */
    public static void reloadTables(String configDir) throws Exception {

        ActorActionCombatStateTableManager.getInstance().load(configDir);

        ActorActionStateTableManager.getInstance().load(configDir);

        BuffTableManager.getInstance().load(configDir);

        ClassTableManager.getInstance().load(configDir);

        ConditionTableManager.getInstance().load(configDir);

        CooldownTableManager.getInstance().load(configDir);

        GlobalVariableTableManager.getInstance().load(configDir);

        ItemTableManager.getInstance().load(configDir);

        MainSceneTableManager.getInstance().load(configDir);

        MessageLimiterTableManager.getInstance().load(configDir);

        MissionTableManager.getInstance().load(configDir);

        MonsterTableManager.getInstance().load(configDir);

        RewardTableManager.getInstance().load(configDir);

        SceneTableManager.getInstance().load(configDir);

        SkillTableManager.getInstance().load(configDir);

        SkillPermissionTableManager.getInstance().load(configDir);

        TestTableManager.getInstance().load(configDir);

        TestMultiKeyTableManager.getInstance().load(configDir);

        if (loadSuccessCallback != null) {
            loadSuccessCallback.run();
        }
    }
}