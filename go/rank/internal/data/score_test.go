package data

import (
	"errors"
	"strconv"
	"testing"
)

// T4:编码边界。只测小数值时 float64 永远精确,必须测上界(2^21 − 1 与 epoch 当秒)。
func TestEncodeScoreBoundaries(t *testing.T) {
	epochMs := ScoreEpochSec * 1000
	top, err := EncodeScore(MaxRatingCenti, epochMs)
	if err != nil {
		t.Fatalf("上界应可编码: %v", err)
	}
	if top != 1<<53-1 {
		t.Fatalf("上界编码应恰为 2^53 − 1,得到 %d", top)
	}
	if got := DecodeRatingCenti(top); got != MaxRatingCenti {
		t.Fatalf("上界解码应回到 %d,得到 %d", MaxRatingCenti, got)
	}
	// 经 float64 往返(Redis ZSET 的 score 就是 float64)仍精确。
	if back := uint64(float64(top)); back != top {
		t.Fatalf("2^53 − 1 经 float64 往返失真: %d", back)
	}
	if _, err := EncodeScore(MaxRatingCenti+1, epochMs); !errors.Is(err, ErrOutOfRange) {
		t.Fatalf("rating_centi = 2^21 必须拒绝,得到 %v", err)
	}
	if _, err := EncodeScore(150000, epochMs-1000); !errors.Is(err, ErrOutOfRange) {
		t.Fatalf("早于 epoch 的时间必须拒绝,得到 %v", err)
	}
	if _, err := EncodeScore(150000, (ScoreEpochSec+1<<32)*1000); !errors.Is(err, ErrOutOfRange) {
		t.Fatalf("超出 32 位秒的时间必须拒绝,得到 %v", err)
	}
	last, err := EncodeScore(0, (ScoreEpochSec+1<<32-1)*1000)
	if err != nil || last != 0 {
		t.Fatalf("0 分 + 最晚可表示的秒应编成 0,得到 %d %v", last, err)
	}
}

// 同分时更早达到的分数更大(ZREVRANGE 排在前面);分数高的永远在分数低的前面,不受时间影响。
func TestEncodeScoreOrdering(t *testing.T) {
	base := (ScoreEpochSec + 1000) * 1000
	early, _ := EncodeScore(150029, base)
	late, _ := EncodeScore(150029, base+5000)
	if early <= late {
		t.Fatalf("同分时早达到的应更大: early=%d late=%d", early, late)
	}
	higherButLater, _ := EncodeScore(150030, base+1_000_000_000)
	if higherButLater <= early {
		t.Fatalf("高 0.01 分的必须排在前面,不论时间: higher=%d early=%d", higherButLater, early)
	}
	if got := DecodeRatingCenti(early); got != 150029 {
		t.Fatalf("解码 rating_centi 应为 150029,得到 %d", got)
	}
}

func TestParseScoreAndVersion(t *testing.T) {
	v, err := ParseScore(strconv.FormatUint(1<<53-1, 10))
	if err != nil || v != 1<<53-1 {
		t.Fatalf("ParseScore 上界: %d %v", v, err)
	}
	for _, bad := range []string{"1.5", "1e10", "-1", "", strconv.FormatUint(1<<53, 10)} {
		if _, err := ParseScore(bad); err == nil {
			t.Errorf("ParseScore(%q) 应报错", bad)
		}
	}
	ver, err := ParseVersion("1800000000123:42")
	if err != nil || ver != (Version{Gen: 1800000000123, Games: 42}) || ver.String() != "1800000000123:42" {
		t.Fatalf("ParseVersion 往返失败: %+v %v", ver, err)
	}
	for _, bad := range []string{"", "12", "a:1", "1:b", ":1"} {
		if _, err := ParseVersion(bad); err == nil {
			t.Errorf("ParseVersion(%q) 应报错", bad)
		}
	}
}
