package sqldialect

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// 纯单测:不连库。Detect 本身(真发一条 SELECT)由 data 包的真库用例覆盖 ——
// MySQL:TestSQLDialectDetect_OnRealMySQL;TiDB:TestTiDBRecommend_* 的入口。

func TestDialect_NamesAndAll(t *testing.T) {
	all := All()
	require.Len(t, all, int(dialectCount))
	for i, d := range all {
		assert.Equal(t, Dialect(i), d, "All() 必须按枚举值升序、每种方言一次")
		assert.NotEmpty(t, dialectNames[d], "方言 %d 没有名字:加了枚举项却没补 dialectNames", uint8(d))
	}
	assert.Equal(t, "mysql", MySQL.String())
	assert.Equal(t, "tidb", TiDB.String())
	assert.Equal(t, "Dialect(200)", Dialect(200).String(), "越界值要打印成看得出是坏值的样子,不能 panic")

	// All() 每次返回新切片:调用方改它不能影响下一次调用。
	all[0] = TiDB
	assert.Equal(t, MySQL, All()[0])
}

func TestFromServerInfo(t *testing.T) {
	const (
		tidbComment  = "TiDB Server (Apache License 2.0) Community Edition, MySQL 8.0 compatible" // 本机 TiDB v8.5.2 原样
		mysqlComment = "MySQL Community Server - GPL"                                             // 本机 MySQL 原样
	)
	cases := []struct {
		name    string
		version string
		comment string
		want    Dialect
	}{
		{"TiDB 默认版本串", "8.0.11-TiDB-v8.5.2", tidbComment, TiDB},
		{"TiDB 老版本", "5.7.25-TiDB-v6.5.0", tidbComment, TiDB},
		{"不分大小写", "8.0.11-tidb-v8.5.2", "", TiDB},
		// 2026-10-13 实测:server-version = "8.0.36" 的 TiDB v8.5.2,VERSION() 里没有 TiDB 字样,只有 version_comment 还认得出。
		{"TiDB 配了 server-version", "8.0.36", tidbComment, TiDB},
		{"只有 VERSION() 认得出(version_comment 为 NULL)", "8.0.11-TiDB-v8.5.2", "", TiDB},
		{"MySQL 8.4", "8.4.3", mysqlComment, MySQL},
		{"本机 mysql:latest", "26.7.0", mysqlComment, MySQL},
		{"发行版打包的 MySQL", "8.0.36-0ubuntu0.22.04.1", "(Ubuntu)", MySQL},
		{"Percona", "8.0.36-28", "Percona Server (GPL), Release 28, Revision 47601f19", MySQL},
		{"两个串都是空的", "", "", MySQL},
	}
	for _, tc := range cases {
		got, err := FromServerInfo(ServerInfo{Version: tc.version, VersionComment: tc.comment})
		require.NoError(t, err, tc.name)
		assert.Equal(t, tc.want, got, "%s:VERSION() = %q, @@version_comment = %q", tc.name, tc.version, tc.comment)
	}
}

func TestFromServerInfo_RejectsMariaDB(t *testing.T) {
	for _, info := range []ServerInfo{
		{Version: "10.11.6-MariaDB-1:10.11.6+maria~ubu2204", VersionComment: "mariadb.org binary distribution"},
		{Version: "11.4.2-MariaDB", VersionComment: ""},
		{Version: "10.6.16", VersionComment: "MariaDB Server"}, // 版本串被改写、只有 version_comment 认得出
	} {
		_, err := FromServerInfo(info)
		require.ErrorIs(t, err, ErrUnsupportedServer, "%s", info)
		// 错误信息要带上两个原始串:运维看到拒启日志时,第一件事是确认对端到底报了什么。
		assert.Contains(t, err.Error(), info.String())
	}
}

func TestServerInfo_String(t *testing.T) {
	info := ServerInfo{Version: "8.0.36", VersionComment: "TiDB Server"}
	assert.Equal(t, `VERSION() = "8.0.36", @@version_comment = "TiDB Server"`, info.String())
}
