#pragma once

#include <algorithm>
#include <cstdint>

#include "proto/common/database/player_cache.pb.h"

// 从 PlayerAllData 取 save_seq 的播种值(docs/design/db-write-fencing-mysql.md §7.2)。单独成头,让 save_seq.h 保持不依赖
// protobuf:save_seq.h 经 core/system/id_generator.h 被大量编译单元引入,只有加载入口与单测需要这一个函数。
namespace save_seq
{
	// 加载时的种子:max(PlayerAllData.last_save_seq, 两张子表 body 里的 fence_save_seq)。不看 epoch 是否相同(§7.2)。
	//   父键存在(常态):last_save_seq 是上一次落到 Redis 的那笔存盘的序号;子表围栏为 0(C++ 不写 body 围栏)。
	//   父键缺失、login 从子表缓存或 MySQL 拼出来(冷路径):last_save_seq 为 0;子表围栏是已落库序号的下限(§6.3)。
	inline uint64_t SaveSeqFloorFrom(const PlayerAllData &all)
	{
		return std::max({all.last_save_seq(), all.player_database_data().fence_save_seq(),
						 all.player_database_1_data().fence_save_seq()});
	}
}
