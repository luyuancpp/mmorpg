
package com.game.table;

import java.util.List;

/**
 * Auto-generated per-column component records for BuffTable.
 * DO NOT EDIT — regenerate from Excel via Data Table Exporter.
 */
public final class BuffTableComp {

    private BuffTableComp() {}

    // ============================================================
    // Scalar columns → value records
    // Repeated columns → list records
    // ============================================================


    public record Id(int value) {
        public static Id from(BuffTable row) {
            return new Id(row.getId());
        }
    }

    public record No_caster(int value) {
        public static No_caster from(BuffTable row) {
            return new No_caster(row.getNoCaster());
        }
    }

    public record Buff_type(int value) {
        public static Buff_type from(BuffTable row) {
            return new Buff_type(row.getBuffType());
        }
    }

    public record Level(int value) {
        public static Level from(BuffTable row) {
            return new Level(row.getLevel());
        }
    }

    public record Max_layer(int value) {
        public static Max_layer from(BuffTable row) {
            return new Max_layer(row.getMaxLayer());
        }
    }

    public record Infinite_duration(int value) {
        public static Infinite_duration from(BuffTable row) {
            return new Infinite_duration(row.getInfiniteDuration());
        }
    }

    public record Duration(double value) {
        public static Duration from(BuffTable row) {
            return new Duration(row.getDuration());
        }
    }

    public record Force_interrupt(int value) {
        public static Force_interrupt from(BuffTable row) {
            return new Force_interrupt(row.getForceInterrupt());
        }
    }

    public record Interval(double value) {
        public static Interval from(BuffTable row) {
            return new Interval(row.getInterval());
        }
    }

    public record Interval_count(int value) {
        public static Interval_count from(BuffTable row) {
            return new Interval_count(row.getIntervalCount());
        }
    }

    public record Movement_speed_boost(double value) {
        public static Movement_speed_boost from(BuffTable row) {
            return new Movement_speed_boost(row.getMovementSpeedBoost());
        }
    }

    public record Movement_speed_reduction(double value) {
        public static Movement_speed_reduction from(BuffTable row) {
            return new Movement_speed_reduction(row.getMovementSpeedReduction());
        }
    }

    public record Health_regeneration(String value) {
        public static Health_regeneration from(BuffTable row) {
            return new Health_regeneration(row.getHealthRegeneration());
        }
    }

    public record Combat_idle_seconds(double value) {
        public static Combat_idle_seconds from(BuffTable row) {
            return new Combat_idle_seconds(row.getCombatIdleSeconds());
        }
    }

    public record Time(int value) {
        public static Time from(BuffTable row) {
            return new Time(row.getTime());
        }
    }

    public record Bonus_damage(String value) {
        public static Bonus_damage from(BuffTable row) {
            return new Bonus_damage(row.getBonusDamage());
        }
    }

    public record Interval_effect(List<Double> values) {
        public static Interval_effect from(BuffTable row) {
            return new Interval_effect(row.getIntervalEffectList());
        }
    }

    public record Sub_buff(List<Integer> values) {
        public static Sub_buff from(BuffTable row) {
            return new Sub_buff(row.getSubBuffList());
        }
    }

    public record Target_sub_buff(List<Integer> values) {
        public static Target_sub_buff from(BuffTable row) {
            return new Target_sub_buff(row.getTargetSubBuffList());
        }
    }

}