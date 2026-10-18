// Package savebarrier 定义父快照与两张 SQL 子表共用的提交屏障版本。
package savebarrier

import (
	"fmt"
	"strconv"
)

// Key 不设 TTL；恢复或合服时必须与父快照、SQL 提交证据一起处理。
func Key(playerID uint64) string {
	return fmt.Sprintf("player:{%d}:save_barrier", playerID)
}

// Version 的两段各占 20 字节，中间冒号，总长 41；逐字节顺序等于整数元组顺序。
func Version(ownerEpoch, saveSeq uint64) string {
	return fmt.Sprintf("%020d:%020d", ownerEpoch, saveSeq)
}

// Parse 拒绝非规范格式和 uint64 溢出，避免不同写者对同一个令牌作不同解释。
func Parse(version string) (ownerEpoch, saveSeq uint64, err error) {
	if len(version) != 41 || version[20] != ':' {
		return 0, 0, fmt.Errorf("存盘屏障版本格式错误")
	}
	for i := range version {
		if i != 20 && (version[i] < '0' || version[i] > '9') {
			return 0, 0, fmt.Errorf("存盘屏障版本包含非数字")
		}
	}
	ownerEpoch, err = strconv.ParseUint(version[:20], 10, 64)
	if err != nil {
		return 0, 0, fmt.Errorf("存盘屏障归属代次越界: %w", err)
	}
	saveSeq, err = strconv.ParseUint(version[21:], 10, 64)
	if err != nil {
		return 0, 0, fmt.Errorf("存盘屏障存盘序号越界: %w", err)
	}
	return ownerEpoch, saveSeq, nil
}

// LuaVersionHelpers 禁用 tonumber 和受 locale 影响的字符串大小比较。
// 调用方必须在任何 Redis 写入之前校验所有输入和存量字段；脚本错误不会回滚已做的写入。
const LuaVersionHelpers = `
local function version_gt(a, b)
    for i = 1, #a do
        local x, y = string.byte(a, i), string.byte(b, i)
        if x ~= y then return x > y end
    end
    return false
end
local function is_version(s)
    if type(s) ~= 'string' or #s ~= 41 or string.sub(s, 21, 21) ~= ':'
        or not string.match(s, '^%d+:%d+$') then return false end
    local max = '18446744073709551615'
    return not version_gt(string.sub(s, 1, 20), max)
        and not version_gt(string.sub(s, 22, 41), max)
end
`
