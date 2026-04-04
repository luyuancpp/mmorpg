#include "condition_util.h"

#include <algorithm>

#include "table/code/condition_table.h"

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

bool IsFulfilled(uint32_t conditionId, uint32_t progressValue, uint32_t targetCount) {
	FetchConditionTableOrReturnFalse(conditionId);
	const auto cmpIndex = static_cast<size_t>(conditionTable->comparison_op());
	if (cmpIndex >= kComparisonFunctionCount) return false;
	return kComparisonFunctions[cmpIndex](progressValue, targetCount);
}

bool MatchesEventSlots(const ConditionTable* conditionRow,
                       const google::protobuf::RepeatedField<uint32_t>& eventConditionIds) {
	size_t configSlotCount = 0;
	size_t matchedSlotCount = 0;

	auto matchSlot = [&](const auto& slotValues, size_t slotIndex) {
		if (slotValues.empty()) {
			return;
		}
		++configSlotCount;
		if (slotIndex >= static_cast<size_t>(eventConditionIds.size())) {
			return;
		}
		const auto eventId = eventConditionIds.Get(static_cast<int>(slotIndex));
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

uint32_t ClampIfFulfilled(uint32_t conditionId, uint32_t progress, uint32_t targetCount) {
	if (IsFulfilled(conditionId, progress, targetCount)) {
		return std::min(progress, targetCount);
	}
	return progress;
}

} // namespace condition_util
