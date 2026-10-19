#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""排行榜 rank(docs/design/leaderboard-system.md §4.5 / §5.4 第 2 步)要对 data/MessageLimiter.xlsx 做的改动,写成可重复执行的脚本。

rank v1 没有 tip 段(复用 common 段,§4.5),所以只有一个子命令:

    python tools/scripts/rank_xlsx_patch.py message-limiter   # 全量 proto-gen 之后、导表之前

给 ClientPlayerRank.GetRankPage 登记 5 次 / 秒(照 team 的查询档,§4.5)。不配档位就吃 gate 默认的"1 秒 3 次",
客户端翻页偶发被拒;被限频时客户端不得自动重试(gate IllegalPacketCounter 会累计并 forceClose)。

为什么是脚本、为什么必须在 proto-gen 之后:与 tools/scripts/mail_xlsx_patch.py 文件头逐条相同 ——
MessageLimiter.xlsx 是多个会话同时写的二进制表,整表写回会静默删掉别人的行;档位按**数字消息号**登记,
号要等全量 proto-gen 才定死。写表的通用机制(只写空格、保存前核对文件没被并发改过、保存后逐格读回、
用导表器自己的 schema 绑定器前后各核一次)全部复用 mail_xlsx_patch.py,本文件只描述 rank 的那一行。

幂等:已经是目标状态就只报告、不改文件;发现那个号上已有一行不同的档位就中止,绝不覆盖。加 --dry-run 只打印不写。
执行前先确认 `git status --short data/MessageLimiter.xlsx` 没有别人的未提交改动。
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import openpyxl  # noqa: E402  mail_xlsx_patch 在 import 时已检查过 openpyxl 是否安装

from mail_xlsx_patch import (  # noqa: E402  通用机制,见文件头
    DATA_BEGIN_ROW,
    LIMITER_COLUMNS,
    LIMITER_SHEET,
    LIMITER_TIME_WINDOW,
    LIMITER_TIP_MESSAGE,
    LIMITER_XLSX,
    MESSAGE_ID_TXT,
    Patch,
    apply_and_verify,
    bind_limiter_schema,
    fail,
    file_digest,
    header_cells,
    only_sheet,
    rel,
    sheet_matrix,
)

# §4.5 限频档位(次/秒)。改内容只改这一段。tip_message 列与表内既有行一致(理由见 mail_xlsx_patch.LIMITER_TIP_MESSAGE)。
LIMITER_SERVICE = "ClientPlayerRank"
LIMITER_METHODS = {
    "GetRankPage": 5,   # 查询类,照 team 的查询档;客户端单请求在途,翻页 + 打开窗口不会超
}


def resolve_message_ids() -> dict[str, int]:
    """方法名 -> 消息号。查不到就中止 —— 此时还没有打开过 xlsx,什么都不会写。"""
    if not MESSAGE_ID_TXT.is_file():
        fail(f"找不到 {rel(MESSAGE_ID_TXT)} —— 消息号表由全量 proto-gen 生成,先跑 proto-gen。未写任何文件。")
    ids: dict[str, int] = {}
    owner: dict[int, str] = {}
    for line_no, line in enumerate(MESSAGE_ID_TXT.read_text(encoding="utf-8").splitlines(), start=1):
        line = line.strip()
        if not line:
            continue
        number, sep, key = line.partition("=")
        if not sep or not number.isdigit():
            fail(f"{MESSAGE_ID_TXT.name}:{line_no} 不是 `<号>=<服务名+方法名>` 形状:{line!r}")
        if key in ids or int(number) in owner:
            fail(f"{MESSAGE_ID_TXT.name}:{line_no} 的 {line!r} 重复(同名或同号)—— 消息号表坏了,先修。未写任何文件。")
        ids[key] = int(number)
        owner[int(number)] = key
    missing = [LIMITER_SERVICE + m for m in LIMITER_METHODS if LIMITER_SERVICE + m not in ids]
    if missing:
        fail(f"{MESSAGE_ID_TXT.name} 里找不到 {missing} —— 含 rank 的全量 proto-gen 还没跑(或没跑成功),消息号未定,"
             f"现在不能填档位。未写任何文件。")
    return {m: ids[LIMITER_SERVICE + m] for m in LIMITER_METHODS}


def cmd_message_limiter(dry_run: bool) -> None:
    message_ids = resolve_message_ids()
    bind_limiter_schema(after_write=False)

    loaded_digest = file_digest(LIMITER_XLSX)
    wb = openpyxl.load_workbook(LIMITER_XLSX)
    ws = only_sheet(wb, LIMITER_XLSX, LIMITER_SHEET)
    before = sheet_matrix(ws)
    header = header_cells(ws)
    for name in LIMITER_COLUMNS:
        if name not in header:
            fail(f"{LIMITER_XLSX.name} 第 1 行找不到列 {name!r},实际表头 {list(header)}")

    existing: dict[int, tuple] = {}
    last_data_row = DATA_BEGIN_ROW - 1
    for row_idx in range(DATA_BEGIN_ROW, ws.max_row + 1):
        value = ws.cell(row=row_idx, column=header["id"]).value
        if value is None or value == "":
            continue
        if isinstance(value, bool) or not isinstance(value, (int, float)) or int(value) != value:
            fail(f"{LIMITER_XLSX.name} 第 {row_idx} 行的 id 不是整数:{value!r}")
        if int(value) in existing:
            fail(f"{LIMITER_XLSX.name} 里消息号 {int(value)} 出现了两次(第 {row_idx} 行)—— 先修表")
        existing[int(value)] = tuple(ws.cell(row=row_idx, column=header[name]).value for name in LIMITER_COLUMNS[1:])
        last_data_row = row_idx

    to_append = []
    for method, max_requests in LIMITER_METHODS.items():
        message_id = message_ids[method]
        wanted = (max_requests, LIMITER_TIME_WINDOW, LIMITER_TIP_MESSAGE)
        if message_id not in existing:
            to_append.append((message_id, method, wanted))
        elif existing[message_id] != wanted:
            fail(f"消息号 {message_id}({LIMITER_SERVICE}{method})在表里已有一行 {existing[message_id]},"
                 f"与要写的 {wanted} 不同 —— 先确认那一行是谁的,本脚本不覆盖。未写任何文件。")

    if not to_append:
        print(f"[ALREADY CURRENT] All {len(message_ids)} rank message IDs exist in the table; file unchanged.")
        return

    patches: list[Patch] = []
    print(f"Existing data: {len(existing)} rows; last row={last_data_row}; will append {len(to_append)} rows:")
    for offset, (message_id, method, wanted) in enumerate(to_append, start=1):
        row_idx = last_data_row + offset
        patches.append((row_idx, header["id"], message_id))
        patches += [(row_idx, header[name], value) for name, value in zip(LIMITER_COLUMNS[1:], wanted)]
        print(f"  Row {row_idx}: id={message_id:<4} {LIMITER_SERVICE}{method:<14} "
              f"max_requests={wanted[0]} time_window={wanted[1]} tip_message={wanted[2]}")
    if dry_run:
        print("[dry-run] No files written.")
        return
    apply_and_verify(LIMITER_XLSX, LIMITER_SHEET, wb, ws, before, patches, (), loaded_digest)
    bind_limiter_schema(after_write=True)
    print(f"[DONE] Data rows {len(existing)} -> {len(existing) + len(to_append)}; existing cells unchanged. Rerun the table exporter;"
          f" acceptance: the ID above appears in generated/tables/messagelimiter.json. Reload gate MessageLimiter during deployment.")


COMMANDS = {
    "message-limiter": cmd_message_limiter,
}


def self_check() -> None:
    """常量表自检。"""
    for method, tier in LIMITER_METHODS.items():
        if not isinstance(tier, int) or tier <= 0:
            fail(f"内部错误:{method} 的档位必须是正整数")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("command", choices=tuple(COMMANDS))
    parser.add_argument("--dry-run", action="store_true", help="只打印将要做什么,不写文件")
    args = parser.parse_args()
    self_check()
    COMMANDS[args.command](args.dry_run)


if __name__ == "__main__":
    main()
