// ---------------------------------------------------------------------------
//  由 Data Table Exporter 从 EquipEnhance.xlsx 生成。**不要手改**。
//  重新生成:tools/data_table_exporter/run.py
// ---------------------------------------------------------------------------
//
//  与 C++ 那门的差别只有一个:C++ 侧每张表是进程级单例(XxxTableManager::Instance()),
//  UE 侧表挂在 UConfigSubsystem(每个 GameInstance 一份)上,所以每个助手的第一个
//  参数是那个 subsystem。传 nullptr 一律返回空结果,不崩。
//
//  返回的 const FCfgEquipEnhanceRow* / const FCfg*Row* 属于**目标表当前快照**,
//  只在目标表下一次 Load 之前有效。调用方只存 id。
//
//  模块依赖(Build.cs)见 ConfigSubsystem.h 顶部:
//  PublicDependencyModuleNames 要有 Core / CoreUObject / Engine / Json / JsonUtilities。
// ---------------------------------------------------------------------------
#pragma once

#include "CoreMinimal.h"

#include "ConfigSubsystem.h"
#include "EquipEnhanceTable.h"
#include "ItemTable.h"

/// 解析 EquipEnhance.material_id -> Item 行。
inline const FCfgItemRow* GetEquipEnhanceMaterialIdRow(const UConfigSubsystem* Config, const FCfgEquipEnhanceRow& Row)
{
	const UItemTable* Target = Config != nullptr ? Config->GetItemTable() : nullptr;
	return Target != nullptr ? Target->FindByIdSilent(Row.material_id) : nullptr;
}

/// 同上,按 EquipEnhance 的 id 取行再解析。
inline const FCfgItemRow* GetEquipEnhanceMaterialIdRow(const UConfigSubsystem* Config, int32 TableId)
{
	const UEquipEnhanceTable* Source = Config != nullptr ? Config->GetEquipEnhanceTable() : nullptr;
	const FCfgEquipEnhanceRow* Row = Source != nullptr ? Source->FindByIdSilent(TableId) : nullptr;
	return Row != nullptr ? GetEquipEnhanceMaterialIdRow(Config, *Row) : nullptr;
}

// ---------------------------------------------------------------------------
// 反向外键(HasMany):按外键列的值找源表的行
// ---------------------------------------------------------------------------

/// 反查:material_id == Key 的全部 EquipEnhance 行。
inline TArray<const FCfgEquipEnhanceRow*> FindEquipEnhanceRowsByMaterialId(const UConfigSubsystem* Config, int32 Key)
{
	const UEquipEnhanceTable* Source = Config != nullptr ? Config->GetEquipEnhanceTable() : nullptr;
	return Source != nullptr ? Source->GetByMaterialId(Key) : TArray<const FCfgEquipEnhanceRow*>();
}
