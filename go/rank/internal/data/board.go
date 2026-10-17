package data

import (
	"context"
	"errors"
	"fmt"
	"strconv"
	"strings"

	"github.com/zeromicro/go-zero/core/stores/redis"
)

// Snapshot 是一条写榜输入(来自 contracts.kafka.PlayerRatingSnapshotEvent,§2.4)。
type Snapshot struct {
	PlayerID    uint64
	RatingCenti uint32
	Games       uint64
	UpdatedAtMs uint64
	RatingGen   uint64
}

// WriteOutcome 是一次写榜的结果(对应指标 rank_write_total 的 outcome)。
type WriteOutcome int

const (
	// WriteStale:版本不新于已有版本(幂等重放 / 乱序旧事件 / 旧代际残留),什么都没改。
	WriteStale WriteOutcome = 0
	// WriteWritten:同代际、games 更大(或首次上榜),已写入。
	WriteWritten WriteOutcome = 1
	// WriteGenReset:match 侧评分键丢过、代际变大,新代际直接覆盖(不管 games 大小,§3.2 第 2 步)。
	WriteGenReset WriteOutcome = 3
)

// ErrInvalidSnapshot 表示快照字段不满足 match 的对外承诺(player_id / games / rating_gen 为 0):调用方缺陷,拒写,不重试。
var ErrInvalidSnapshot = errors.New("rank: invalid snapshot")

// ErrMalformedVersion 表示版本表里**这一个玩家**的值形状坏了(有人手写过):写 Lua 回 error_reply,不猜新旧。
// 只坏一个成员,消费者把它当永久错误跳过并计数,不卡分区;整键类型错(WRONGTYPE)不归这里,那是所有写都失败,要卡住等人修。
var ErrMalformedVersion = errors.New("rank: malformed version")

// malformedVersionReply 是写 Lua 的 error_reply 前缀,Write 据它把 Redis 错误归成 ErrMalformedVersion。
// 与 writeScript 里的两处字面量逐字一致(board_test.go 钉住)。
const malformedVersionReply = "rank: malformed version"

// writeScript 是单条写榜 Lua(§3.2)。KEYS = [scores ZSET, versions HASH](同一 hash tag,同 slot);
// ARGV = [member, rating_gen, games, rating_centi, compositeScore, versionString]。
//
//  1. HGET 版本:存在且 (gen, games) <= (ogen, ogames)(字典序)→ 返回 0(stale)。
//  2. gen > ogen → 记 gen_reset(返回 3),新代际直接覆盖 ZSET(不保留旧代际的时间分量:旧代际的"达到时刻"属于丢失前的历史)。
//  3. 同代际:ZSCORE 存在且 floor(old / 2^32) == rating_centi → **不改 ZSET**,保留"最初达到该分"的时间
//     (match 每局都推进 updated_at_ms,0 分下限钳住、Δ=0 的平局都会出现"分没变、时间前移");否则 ZADD。
//  4. HSET 版本 → 返回 1(written)或 3(gen_reset)。
//
// 版本值形状坏了(有人手写过)直接 error_reply:不猜它的新旧;消费者把这类错误当永久错误跳过并计数,不卡分区。
// Lua 的数字是 double:gen(毫秒时间戳 ≈ 1.8e12)、games、score(< 2^53)都在精确范围内。
const writeScript = `
local cur = redis.call('HGET', KEYS[2], ARGV[1])
local gen = tonumber(ARGV[2])
local games = tonumber(ARGV[3])
local outcome = 1
if cur then
  local sep = string.find(cur, ':', 1, true)
  if not sep then
    return redis.error_reply('rank: malformed version ' .. cur)
  end
  local ogen = tonumber(string.sub(cur, 1, sep - 1))
  local ogames = tonumber(string.sub(cur, sep + 1))
  if ogen == nil or ogames == nil then
    return redis.error_reply('rank: malformed version ' .. cur)
  end
  if gen < ogen or (gen == ogen and games <= ogames) then
    return 0
  end
  if gen > ogen then
    outcome = 3
  end
end
local keepTime = false
if outcome == 1 then
  local old = redis.call('ZSCORE', KEYS[1], ARGV[1])
  if old and math.floor(tonumber(old) / 4294967296) == tonumber(ARGV[4]) then
    keepTime = true
  end
end
if not keepTime then
  redis.call('ZADD', KEYS[1], ARGV[5], ARGV[1])
end
redis.call('HSET', KEYS[2], ARGV[1], ARGV[6])
return outcome
`

// readScript 是一次读一页 + self 的只读 Lua(§4.2 推荐写法:一次往返,同时消除页内不一致)。
// KEYS = [scores ZSET, versions HASH];ARGV = [page(≥1), page_size(≥1), self member 或 ""]。
// 页码在这里按榜长钳制(超过末页按末页;空榜 page_count = 1):页码、页长都已在 Go 侧钳过上限,
// (page − 1) × size ≤ 2^32 × 50 < 2^53,double 精确。
// 返回 {total, page, page_count, {member, score, ...}, {version, ...}, self_rank0(-1 = 未上榜), self_score, self_version}。
const readScript = `
local total = redis.call('ZCARD', KEYS[1])
local size = tonumber(ARGV[2])
local pageCount = math.floor((total + size - 1) / size)
if pageCount < 1 then
  pageCount = 1
end
local page = tonumber(ARGV[1])
if page > pageCount then
  page = pageCount
end
local rows = {}
local vers = {}
if total > 0 then
  local start = (page - 1) * size
  rows = redis.call('ZREVRANGE', KEYS[1], start, start + size - 1, 'WITHSCORES')
  local members = {}
  for i = 1, #rows, 2 do
    members[#members + 1] = rows[i]
  end
  if #members > 0 then
    vers = redis.call('HMGET', KEYS[2], unpack(members))
  end
end
local selfRank = -1
local selfScore = ''
local selfVer = ''
if ARGV[3] ~= '' then
  local r = redis.call('ZREVRANK', KEYS[1], ARGV[3])
  if r then
    selfRank = r
    selfScore = redis.call('ZSCORE', KEYS[1], ARGV[3])
    selfVer = redis.call('HGET', KEYS[2], ARGV[3]) or ''
  end
end
return {total, page, pageCount, rows, vers, selfRank, selfScore, selfVer}
`

var (
	writeLua = redis.NewScript(writeScript)
	readLua  = redis.NewScript(readScript)
)

// Board 是 match 评分全服榜的读写入口(v1 只有这一张榜,§3.4)。
type Board struct {
	rds      *redis.Redis
	scores   string
	versions string
}

// NewRatingBoard 建 match 评分全服榜。rds 是 RankRedis(或回落的共享库,见 svc)。
func NewRatingBoard(rds *redis.Redis) *Board {
	return &Board{rds: rds, scores: RatingScoresKey(), versions: RatingVersionsKey()}
}

// Write 写一条快照。字段非法 → ErrInvalidSnapshot;编码越界 → ErrOutOfRange;都在发 Lua 之前判,不碰 Redis。
func (b *Board) Write(ctx context.Context, s Snapshot) (WriteOutcome, error) {
	if s.PlayerID == 0 || s.Games == 0 || s.RatingGen == 0 {
		return WriteStale, fmt.Errorf("%w: player_id=%d games=%d rating_gen=%d", ErrInvalidSnapshot, s.PlayerID, s.Games, s.RatingGen)
	}
	score, err := EncodeScore(s.RatingCenti, s.UpdatedAtMs)
	if err != nil {
		return WriteStale, err
	}
	member := strconv.FormatUint(s.PlayerID, 10)
	version := Version{Gen: s.RatingGen, Games: s.Games}
	res, err := b.rds.ScriptRunCtx(ctx, writeLua, []string{b.scores, b.versions},
		member, strconv.FormatUint(s.RatingGen, 10), strconv.FormatUint(s.Games, 10),
		strconv.FormatUint(uint64(s.RatingCenti), 10), strconv.FormatUint(score, 10), version.String())
	if err != nil {
		if strings.Contains(err.Error(), malformedVersionReply) {
			return WriteStale, fmt.Errorf("%w: player=%d: %v", ErrMalformedVersion, s.PlayerID, err)
		}
		return WriteStale, fmt.Errorf("rank: write snapshot player=%d: %w", s.PlayerID, err)
	}
	code, ok := res.(int64)
	if !ok {
		return WriteStale, fmt.Errorf("rank: write snapshot player=%d: unexpected Lua reply %T", s.PlayerID, res)
	}
	switch WriteOutcome(code) {
	case WriteStale, WriteWritten, WriteGenReset:
		return WriteOutcome(code), nil
	}
	return WriteStale, fmt.Errorf("rank: write snapshot player=%d: unexpected Lua outcome %d", s.PlayerID, code)
}

// Entry 是榜上一行(读回时已解码,客户端永不见复合分)。Rank 从 1 起;0 表示未上榜(只出现在 Page.Self)。
type Entry struct {
	Rank        uint32
	PlayerID    uint64
	RatingCenti uint32
	Games       uint64
}

// Page 是一次读榜的结果。Page / PageCount 是按榜长钳制后的值;空榜时 PageCount = 1、Page = 1、Entries 为空。
type Page struct {
	Total     uint64
	Page      uint32
	PageSize  uint32
	PageCount uint32
	Entries   []Entry
	Self      Entry // Self.Rank == 0 表示未上榜(不是错误)
}

// ReadPage 读一页 + self(selfID = 0 表示不查 self)。page ≥ 1、1 ≤ pageSize 由调用方钳好(logic 层)。
func (b *Board) ReadPage(ctx context.Context, page, pageSize uint32, selfID uint64) (Page, error) {
	if page == 0 || pageSize == 0 {
		return Page{}, fmt.Errorf("rank: ReadPage page=%d page_size=%d must be >= 1", page, pageSize)
	}
	self := ""
	if selfID != 0 {
		self = strconv.FormatUint(selfID, 10)
	}
	res, err := b.rds.ScriptRunCtx(ctx, readLua, []string{b.scores, b.versions},
		strconv.FormatUint(uint64(page), 10), strconv.FormatUint(uint64(pageSize), 10), self)
	if err != nil {
		return Page{}, fmt.Errorf("rank: read page: %w", err)
	}
	return decodeReadReply(res, pageSize, selfID)
}

// decodeReadReply 解析 readScript 的返回值。任何形状不对都是 error(fail-closed:不把半份数据当成榜)。
func decodeReadReply(res any, pageSize uint32, selfID uint64) (Page, error) {
	arr, ok := res.([]any)
	if !ok || len(arr) != 8 {
		return Page{}, fmt.Errorf("rank: read page: unexpected Lua reply %T len=%d", res, lenOf(res))
	}
	total, err1 := asInt64(arr[0])
	page, err2 := asInt64(arr[1])
	pageCount, err3 := asInt64(arr[2])
	if err := errors.Join(err1, err2, err3); err != nil {
		return Page{}, fmt.Errorf("rank: read page header: %w", err)
	}
	if total < 0 || page < 1 || pageCount < 1 || page > pageCount || pageCount > 1<<32-1 {
		return Page{}, fmt.Errorf("rank: read page header out of range total=%d page=%d page_count=%d", total, page, pageCount)
	}
	out := Page{Total: uint64(total), Page: uint32(page), PageSize: pageSize, PageCount: uint32(pageCount)}

	rows, _ := arr[3].([]any)
	vers, _ := arr[4].([]any)
	if len(rows)%2 != 0 || (len(rows) > 0 && len(vers) != len(rows)/2) {
		return Page{}, fmt.Errorf("rank: read page: %d row fields vs %d versions", len(rows), len(vers))
	}
	firstRank := uint64(page-1)*uint64(pageSize) + 1
	for i := 0; i+1 < len(rows); i += 2 {
		entry, err := decodeEntry(rows[i], rows[i+1], vers[i/2])
		if err != nil {
			return Page{}, err
		}
		rank := firstRank + uint64(i/2)
		if rank > 1<<32-1 {
			return Page{}, fmt.Errorf("rank: rank %d overflows uint32", rank)
		}
		entry.Rank = uint32(rank)
		out.Entries = append(out.Entries, entry)
	}

	selfRank, err := asInt64(arr[5])
	if err != nil {
		return Page{}, fmt.Errorf("rank: read self rank: %w", err)
	}
	if selfRank >= 0 {
		entry, err := decodeEntry(strconv.FormatUint(selfID, 10), arr[6], arr[7])
		if err != nil {
			return Page{}, err
		}
		if uint64(selfRank)+1 > 1<<32-1 {
			return Page{}, fmt.Errorf("rank: self rank %d overflows uint32", selfRank)
		}
		entry.Rank = uint32(selfRank + 1)
		out.Self = entry
	}
	return out, nil
}

// decodeEntry 把 (member, score, version) 解成一行。版本缺失(只有 ZSET 没有 HASH,§3.5 的半丢失状态)时 games 记 0、不报错:
// 名次与分数仍可信,局数只是展示数据。member / score 形状不对才报错。
func decodeEntry(memberRaw, scoreRaw, versionRaw any) (Entry, error) {
	member, ok := memberRaw.(string)
	if !ok {
		return Entry{}, fmt.Errorf("rank: unexpected member %T", memberRaw)
	}
	playerID, err := strconv.ParseUint(member, 10, 64)
	if err != nil || playerID == 0 {
		return Entry{}, fmt.Errorf("rank: unexpected member %q", member)
	}
	scoreStr, ok := scoreRaw.(string)
	if !ok {
		return Entry{}, fmt.Errorf("rank: unexpected score %T for player %d", scoreRaw, playerID)
	}
	score, err := ParseScore(scoreStr)
	if err != nil {
		return Entry{}, err
	}
	entry := Entry{PlayerID: playerID, RatingCenti: DecodeRatingCenti(score)}
	if raw, ok := versionRaw.(string); ok && raw != "" {
		if v, err := ParseVersion(raw); err == nil {
			entry.Games = v.Games
		}
	}
	return entry, nil
}

// Size 是榜长(ZCARD),供 rank_board_size 采样。
func (b *Board) Size(ctx context.Context) (int, error) {
	return b.rds.ZcardCtx(ctx, b.scores)
}

func asInt64(v any) (int64, error) {
	switch x := v.(type) {
	case int64:
		return x, nil
	case string:
		return strconv.ParseInt(x, 10, 64)
	}
	return 0, fmt.Errorf("unexpected %T", v)
}

func lenOf(v any) int {
	if arr, ok := v.([]any); ok {
		return len(arr)
	}
	return -1
}
