// Package sqldialect 判断连接池背后那台 SQL 服务器是 MySQL 还是 TiDB。
//
// friend 的表与绝大多数语句在 MySQL 和 TiDB 上写法相同(数据层迁 TiDB 是既定方向,见 AGENTS §1),
// 只有一处必须分开写:**优化器提示**。两边的提示名互不相认 —— MySQL 的 SEMIJOIN(FIRSTMATCH) 在 TiDB 上是
// 警告 8061 后忽略,TiDB 的 INL_JOIN 在 MySQL 上是 1064 提示语法错 —— 而好友推荐两条查询的扫描上界在两边都要靠
// 提示钉住(MySQL:data/recommend_repo.go 的 RecommendByMutual / recommendAnchor 注释;TiDB:data/recommend_sql.go)。
// 所以提示按方言各写一份,启动期探测一次方言、选定文本,不让任何一边每次调用都带着对方的警告跑。
//
// 为什么单独成包:装配层(svc)要探测方言,数据层(data)要按方言写 SQL,两边都得认识这个类型。
// 本包只依赖标准库,谁都可以放心 import,svc 不必为了一个枚举去**直接** import 数据层;
// 以后别的服务也要按方言写 SQL 时,整包上提到 go/shared 即可。
// (这不等于 svc 与 data 无关:svc import config,config 取 data.DatabaseName,所以 svc 经 config 本来就间接依赖
// data 以及 data 依赖的 metrics。那条链不是本包引入的,也没有被本包消掉。)
package sqldialect

import (
	"context"
	"database/sql"
	"errors"
	"fmt"
	"strings"
)

// Dialect 是一种 SQL 方言。底层类型按"方言个数"选(AGENTS §11.2):与任何 id 的位宽无关。
type Dialect uint8

const (
	// MySQL 是零值:手工构造、没有经过启动期探测的装配(单测里的假装配)按 MySQL 处理 ——
	// 本仓库的集成测试与本地、K8s 部署目前都跑 MySQL。
	MySQL Dialect = iota
	TiDB

	// dialectCount 固定为最后一项,同时是 dialectNames 的长度:加方言时两处不会脱节。
	dialectCount
)

// dialectNames 与上面的枚举一一对应(数组长度由 dialectCount 给出)。名字进启动日志与错误信息,不进指标 label。
var dialectNames = [dialectCount]string{
	MySQL: "mysql",
	TiDB:  "tidb",
}

func (d Dialect) String() string {
	if d < dialectCount {
		return dialectNames[d]
	}
	return fmt.Sprintf("Dialect(%d)", uint8(d))
}

// All 返回全部已定义的方言(按枚举值升序,每次调用一份新切片)。
// 给"每种方言都要有一份"的地方做穷举用 —— 例如 data 包里按方言渲染 SQL 的单测。
func All() []Dialect {
	all := make([]Dialect, 0, dialectCount)
	for d := Dialect(0); d < dialectCount; d++ {
		all = append(all, d)
	}
	return all
}

// ServerInfo 是探测时从服务器取回的两个原始串,原样进启动日志。
type ServerInfo struct {
	Version        string // SELECT VERSION()
	VersionComment string // @@version_comment;服务器返回 NULL 时为空串
}

func (s ServerInfo) String() string {
	return fmt.Sprintf("VERSION() = %q, @@version_comment = %q", s.Version, s.VersionComment)
}

// ErrUnsupportedServer 表示对端是本服务明确不支持的 SQL 服务器(目前只有 MariaDB),用 errors.Is 判。
var ErrUnsupportedServer = errors.New("unsupported SQL server")

// FromServerInfo 从两个版本串判方言。**两个串任一个**指向 TiDB 就判 TiDB,理由:
//
//   - VERSION():TiDB 默认形如 "8.0.11-TiDB-v8.5.2" —— 前半段冒充 MySQL 版本号,自己的版本跟在 "-TiDB-" 后面。
//     但 TiDB 有配置项 server-version,会把 VERSION() 与握手包里的版本串整个换掉(常见用途是让挑版本号的客户端 / ORM
//     通过)。2026-10-13 在 v8.5.2 上实测:配成 "8.0.36" 后 VERSION() 就是 "8.0.36",一个 TiDB 字样都没有。
//     只看这一个串,那样的集群会被当成 MySQL:不报错、照常启动,推荐查询跑的是带 SEMIJOIN 的那份文本 ——
//     每条语句五条 8061,统计偏小时退回整表扫描,正是按方言渲染要消掉的退化。
//   - @@version_comment:TiDB 上是 "TiDB Server (Apache License 2.0) Community Edition, MySQL 8.0 compatible",
//     同一次实测里它不受 server-version 影响;MySQL 上是 "MySQL Community Server - GPL" 这类发行说明。
//
// MariaDB 明确拒绝(返回 ErrUnsupportedServer),而不是当成 MySQL:本服务没有在 MariaDB 上跑过;
// 按 MariaDB 文档,12.0 之前的版本把 `/*+ ... */` 当普通注释整个忽略(本机没有 MariaDB,这一点没有实测) ——
// 那样两条推荐查询的提示与语句超时会全部静默失效。认不出来就不启动,比带着失效的提示上线好查。
//
// 其余一律按 MySQL(Percona、各家云上的 MySQL 兼容版都认 MySQL 的提示语法)。
func FromServerInfo(info ServerInfo) (Dialect, error) {
	version := strings.ToLower(info.Version)
	comment := strings.ToLower(info.VersionComment)
	mentions := func(name string) bool {
		return strings.Contains(version, name) || strings.Contains(comment, name)
	}
	switch {
	case mentions("tidb"):
		return TiDB, nil
	case mentions("mariadb"):
		return MySQL, fmt.Errorf("%w: MariaDB (%s)", ErrUnsupportedServer, info)
	default:
		return MySQL, nil
	}
}

// Detect 向服务器问一次版本信息并判方言,同时把两个原始串返回(启动日志用)。
//
// 由装配层在启动期调一次(svc.NewServiceContext),结果交给 data.NewFriendRepo;请求路径上不重复探测。
// 出错时返回 error 而不是悄悄当成 MySQL:方言判错的后果是"提示全部失效、推荐查询在统计不准时整表扫描",
// 那种退化不报错、只在库大了以后变慢,比启动失败难查得多。出错时返回的 Dialect 没有意义,调用方必须先看 error。
func Detect(ctx context.Context, db *sql.DB) (Dialect, ServerInfo, error) {
	var (
		info    ServerInfo
		comment sql.NullString // 正常不会是 NULL;真是 NULL 时只是少一个判据,不该让探测失败
	)
	if err := db.QueryRowContext(ctx, "SELECT VERSION(), @@version_comment").Scan(&info.Version, &comment); err != nil {
		return MySQL, info, fmt.Errorf("query server version: %w", err)
	}
	info.VersionComment = comment.String
	dialect, err := FromServerInfo(info)
	return dialect, info, err
}
