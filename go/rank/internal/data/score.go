package data

import (
	"errors"
	"fmt"
	"strconv"
	"strings"

	"shared/snowflake"
)

// 复合分数编码(设计 §3.3):Redis ZSET 的 score 是 float64,整数 < 2^53 才精确。
//
//	compositeScore = rating_centi × 2^32 + (2^32 − 1 − (updated_sec − EPOCH))
//	  rating_centi : 分 × 100 的精确整数,取值 [0, 2^21 − 1] = [0, 20971.51]
//	  updated_sec  : updated_at_ms / 1000(与 rating 同源,match 在同一次 Lua 里读出)
//	  EPOCH        : shared/snowflake 的秒级 epoch(1773446400)
//	  最大值       : (2^21 − 1) × 2^32 + (2^32 − 1) = 2^53 − 1 —— 恰好不越界
//
// 时间取补数:同分时"更早"的值更大,ZREVRANGE 排在前面("分数相同,先达到该分者在前",§3.3 / L-9)。
// "先达到"的语义还要靠写入 Lua 的"同分不改时间分量"保证(board.go)。
const (
	ratingBits = 21
	timeBits   = 32

	// MaxRatingCenti 是 rating_centi 的上界(含)。K=32 的 Elo 实际不可能逼近 20971,这是防御。
	MaxRatingCenti = 1<<ratingBits - 1
	// timeSpan 是时间分量能表示的秒数(约 136 年)。
	timeSpan uint64 = 1 << timeBits
)

// ScoreEpochSec 是复合分时间分量的起点(秒)。
const ScoreEpochSec = snowflake.Epoch

// ErrOutOfRange 表示快照的分数或时间超出编码范围:**拒写该条**,不截断 —— 截断会让两个不同分数排出错误顺序且零报错(§3.3)。
var ErrOutOfRange = errors.New("rank: snapshot out of score encoding range")

// EncodeScore 把 (rating_centi, updated_at_ms) 编成复合分。越界返回 ErrOutOfRange。
func EncodeScore(ratingCenti uint32, updatedAtMs uint64) (uint64, error) {
	if ratingCenti > MaxRatingCenti {
		return 0, fmt.Errorf("%w: rating_centi=%d > %d", ErrOutOfRange, ratingCenti, MaxRatingCenti)
	}
	sec := updatedAtMs / 1000
	if sec < ScoreEpochSec || sec-ScoreEpochSec >= timeSpan {
		return 0, fmt.Errorf("%w: updated_at_ms=%d (seconds since epoch must be in [0, 2^32))", ErrOutOfRange, updatedAtMs)
	}
	return uint64(ratingCenti)<<timeBits + (timeSpan - 1 - (sec - ScoreEpochSec)), nil
}

// DecodeRatingCenti 从复合分取回 rating_centi(读回时服务端解码,客户端永不见复合分)。
func DecodeRatingCenti(score uint64) uint32 {
	return uint32(score >> timeBits)
}

// ParseScore 解析 Redis 回的 score 字符串(整数形态;float64 在 < 2^53 内精确,Redis 按 %.17g 输出,不带小数点与指数)。
// 带小数点 / 指数的串说明有人手写过 ZADD,按损坏处理。
func ParseScore(raw string) (uint64, error) {
	v, err := strconv.ParseUint(strings.TrimSpace(raw), 10, 64)
	if err != nil {
		return 0, fmt.Errorf("rank: unexpected ZSET score %q: %w", raw, err)
	}
	if v >= 1<<53 {
		return 0, fmt.Errorf("rank: ZSET score %d exceeds 2^53", v)
	}
	return v, nil
}

// Version 是版本表里的 "<rating_gen>:<games>"(§3.2)。按 (Gen, Games) 字典序比较新旧。
type Version struct {
	Gen   uint64
	Games uint64
}

// String 是写进版本表的形态。
func (v Version) String() string {
	return strconv.FormatUint(v.Gen, 10) + ":" + strconv.FormatUint(v.Games, 10)
}

// ParseVersion 解析版本表的值。形状不对(有人手写过)返回错误,不猜。
func ParseVersion(raw string) (Version, error) {
	gen, games, ok := strings.Cut(raw, ":")
	if !ok {
		return Version{}, fmt.Errorf("rank: malformed version %q", raw)
	}
	g, err := strconv.ParseUint(gen, 10, 64)
	if err != nil {
		return Version{}, fmt.Errorf("rank: malformed version %q: %w", raw, err)
	}
	n, err := strconv.ParseUint(games, 10, 64)
	if err != nil {
		return Version{}, fmt.Errorf("rank: malformed version %q: %w", raw, err)
	}
	return Version{Gen: g, Games: n}, nil
}
