#include "condition_util.h"

#include <algorithm>

#include "table/code/condition_table.h"
#include "proto/common/event/mission_event.pb.h"

namespace {

using ComparisonFn = bool(*)(uint32_t, uint32_t);

constexpr ComparisonFn kComparisonFunctions[] = {
	[](uint32_t actual, uint32_t config) { return actual >= config; },
	[](uint32_t actual, uint32_t config) { return actual >  config; },
	[](uint32_t actual, uint32_t config) { return actual <= config; },
	[](uint32_t actual, uint32_t config) { return actual <  config; },
	[](uint32_t actual, uint32_t config) { return actual == config; },
};

constexpr size_t kComparisonFunctionCount = std::size(kComparisonFunctions);

} // anonymous namespace

namespace condition_util {

bool IsFulfilled(uint32_t conditionId, uint32_t progressValue) {
	FetchConditionTableOrReturnFalse(conditionId);
	const auto cmpIndex = static_cast<size_t>(conditionTable->comparison_op());
	if (cmpIndex >= kComparisonFunctionCount) return false;
	return kComparisonFunctions[cmpIndex](progressValue, conditionTable->target_count());
}

bool MatchesEventSlots(const ConditionTable* conditionRow,
                       const MissionConditionEvent& conditionEvent) {
	size_t configSlotCount = 0;
	size_t matchedSlotCount = 0;

	auto matchSlot = [&](const auto& slotValues, size_t slotIndex) {
		if (slotValues.empty()) {
			return;
		}
		++configSlotCount;
		if (slotIndex >= static_cast<size_t>(conditionEvent.condtion_ids().size())) {
			return;
		}
		const auto eventId = conditionEvent.condtion_ids(static_cast<int>(slotIndex));
		for (int32_t ci = 0; ci < slotValues.size(); ++ci) {
			if (slotValues.Get(ci) == eventId) {
				++matchedSlotCount;
				return;
			}
		}
	};

	matchSlot(conditionRow->condition1(), 0);
	matchSlot(conditionRow->condition2(), 1);
	matchSlot(conditionRow->condition3(), 2);
	matchSlot(conditionRow->condition4(), 3);

	return configSlotCount == 0 || matchedSlotCount == configSlotCount;
}

uint32_t ClampIfFulfilled(uint32_t conditionId, uint32_t progress) {
	FetchConditionTableOrReturnCustom(conditionId, progress);
	if (IsFulfilled(conditionId, progress)) {
		return std::min(progress, conditionTable->target_count());
	}
	return progress;
}

} // namespace condition_util
