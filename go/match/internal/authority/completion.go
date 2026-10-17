package authority

import (
	"bytes"
	"context"
	"crypto/sha256"
	"errors"
	"fmt"
	"sort"
	"time"

	"github.com/go-sql-driver/mysql"
	"google.golang.org/protobuf/proto"
	battlepb "proto/battle"
	kafkapb "proto/contracts/kafka"
)

// 留出Kafka默认消息封装余量；两版生产者使用同一上限，消费者保持1MiB读取预算。
const maxCompletionBytes = 960 * 1024

// ArchiveCompletion 只保全完整结算来源，不投递资产、不确认玩家到账，也不销账。
// 同一事务保存 Kafka 原字节及全部个人原件；重复局只接受完全相同的原字节。
// 原提交结果未知时按原消息重放，不得重算另一份结算。来源位点记录第一次成功归档的位置。
func (s *Store) ArchiveCompletion(ctx context.Context, event *kafkapb.BattleResultEvent, raw []byte, topic string, partition int, offset int64) error {
	if err := validateCompletion(event, raw); err != nil {
		return err
	}
	if event.SettlementFormatVersion == 0 {
		return nil
	}
	if topic == "" || partition < 0 || int64(partition) > int64(1<<31-1) || offset < 0 {
		return errors.New("完整结算来源缺少有效Kafka位点")
	}
	if s == nil || s.DB == nil {
		return errors.New("完整结算来源数据库未配置")
	}
	// 与既有SQL评级相同的单次8秒预算；消费者停机取消仍沿父ctx传播。
	ctx, cancel := context.WithTimeout(ctx, 8*time.Second)
	defer cancel()
	digest := sha256.Sum256(raw)
	tx, err := s.DB.BeginTx(ctx, nil)
	if err != nil {
		return err
	}
	defer tx.Rollback()
	stamp, err := now(ctx, tx)
	if err != nil {
		return err
	}
	_, err = tx.ExecContext(ctx, "INSERT INTO match_battle_completion(battle_id,raw_payload,payload_digest,format_version,archived_ms,source_topic,source_partition,source_offset) VALUES(?,?,?,?,?,?,?,?)",
		event.BattleId, raw, digest[:], event.SettlementFormatVersion, stamp, topic, partition, offset)
	if err != nil {
		var duplicate *mysql.MySQLError
		if !errors.As(err, &duplicate) || duplicate.Number != 1062 {
			return err
		}
		var previous, previousDigest []byte
		var version uint32
		if err := tx.QueryRowContext(ctx, "SELECT raw_payload,payload_digest,format_version FROM match_battle_completion WHERE battle_id=?", event.BattleId).Scan(&previous, &previousDigest, &version); err != nil {
			return err
		}
		if version != event.SettlementFormatVersion || !bytes.Equal(previous, raw) || !bytes.Equal(previousDigest, digest[:]) {
			return errors.New("同对局完整结算来源原字节冲突")
		}
		return nil
	}
	settlements := append([]*battlepb.BattleSettlementData(nil), event.Settlements...)
	sort.Slice(settlements, func(i, j int) bool { return settlements[i].PlayerId < settlements[j].PlayerId })
	for _, settlement := range settlements {
		body, err := proto.MarshalOptions{Deterministic: true}.Marshal(settlement)
		if err != nil {
			return err
		}
		personalDigest := sha256.Sum256(body)
		if _, err := tx.ExecContext(ctx, "INSERT INTO match_battle_settlement_source(battle_id,player_id,settlement_proto,payload_digest,archived_ms) VALUES(?,?,?,?,?)",
			event.BattleId, settlement.PlayerId, body, personalDigest[:], stamp); err != nil {
			return err
		}
	}
	if err := tx.Commit(); err != nil {
		return fmt.Errorf("完整结算来源提交结果未知，必须按原消息重放: %w", err)
	}
	return nil
}

func validateCompletion(event *kafkapb.BattleResultEvent, raw []byte) error {
	if len(raw) > maxCompletionBytes {
		return errors.New("完整结算来源超过960KiB")
	}
	if event == nil {
		return errors.New("对局结果缺失")
	}
	decoded := &kafkapb.BattleResultEvent{}
	if err := proto.Unmarshal(raw, decoded); err != nil || !proto.Equal(decoded, event) {
		return errors.New("结算来源原字节与解析结果不符")
	}
	switch event.SettlementFormatVersion {
	case 0:
		if len(event.Settlements) != 0 {
			return errors.New("旧版结果不能携带未声明完整性的个人结算")
		}
		return nil
	case 1:
	default:
		return errors.New("不支持的完整结算来源版本")
	}
	if event.BattleId == 0 || event.Outcome < battlepb.EBattleOutcome_BATTLE_OUTCOME_SIDE_A_WIN || event.Outcome > battlepb.EBattleOutcome_BATTLE_OUTCOME_DRAW {
		return errors.New("完整结算来源缺少有效对局或终局")
	}
	members := make(map[uint64]uint32)
	teams := make(map[uint32]bool)
	for _, team := range event.Teams {
		if team == nil || teams[team.TeamIndex] || len(team.PlayerIds) == 0 {
			return errors.New("完整结算来源队伍为空或重复")
		}
		teams[team.TeamIndex] = true
		for _, player := range team.PlayerIds {
			if _, exists := members[player]; player == 0 || exists {
				return errors.New("完整结算来源玩家为空或重复")
			}
			members[player] = team.TeamIndex
		}
	}
	if len(members) == 0 || len(event.Settlements) != len(members) {
		return errors.New("完整结算来源缺少全部参战玩家")
	}
	seen := make(map[uint64]bool, len(members))
	for _, settlement := range event.Settlements {
		if settlement == nil || settlement.PlayerId == 0 || seen[settlement.PlayerId] {
			return errors.New("个人结算缺失或玩家重复")
		}
		team, exists := members[settlement.PlayerId]
		if !exists || settlement.BattleId != event.BattleId || settlement.Outcome != event.Outcome || settlement.PlayerTeamIndex != team {
			return errors.New("个人结算与对局、终局或队伍不符")
		}
		seen[settlement.PlayerId] = true
	}
	return nil
}
