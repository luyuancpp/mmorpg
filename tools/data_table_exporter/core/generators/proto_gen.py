"""Proto file generator + protoc compiler.

1. Reads each ``.xlsx`` schema and writes a ``{name}_table.proto`` message.
2. Runs ``protoc`` to compile protos → C++ and Go outputs.
"""

from __future__ import annotations

import logging
import os
import subprocess
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from jinja2 import Environment, FileSystemLoader

from core.config_loader import ExporterConfig
from core.file_utils import ensure_dirs, write_file
from core.schema import TableSchema

logger = logging.getLogger(__name__)


# ---------------------------------------------------------------------------
# .proto file generation
# ---------------------------------------------------------------------------

def generate_proto_files(cfg: ExporterConfig, tables: list[TableSchema]) -> None:
    """Generate ``*_table.proto`` for every data table."""
    ensure_dirs(cfg.proto_dir)
    env = Environment(loader=FileSystemLoader(str(cfg.template_dir), encoding="utf-8"))
    template = env.get_template("proto_table.proto.j2")

    with ThreadPoolExecutor() as pool:
        java_pkg = cfg.java.package if cfg.java.enabled else ""
        pool.map(lambda t: _gen_one_proto(t, template, cfg, java_pkg), tables)


def _gen_one_proto(table: TableSchema, template, cfg: ExporterConfig, java_package: str = "") -> None:
    try:
        content = template.render(table=table, java_package=java_package)
        out = cfg.proto_dir / f"{table.name.lower()}_table.proto"
        write_file(out, content)
        logger.info("Generated %s", out)
    except Exception as exc:
        logger.error("Proto gen failed for %s: %s", table.name, exc)


# ---------------------------------------------------------------------------
# protoc compilation
# ---------------------------------------------------------------------------

def compile_proto_python(cfg: ExporterConfig) -> None:
    """Compile all ``.proto`` -> Python ``*_pb2.py`` for binary serialisation."""
    ensure_dirs(cfg.proto_python_output_dir)
    _compile(cfg.proto_dir, cfg.proto_python_output_dir, "python_out", cfg)


def compile_proto_cpp(cfg: ExporterConfig) -> None:
    """Compile all ``.proto`` → C++ using *protoc*."""
    if not cfg.cpp.enabled:
        return
    ensure_dirs(cfg.cpp.proto_output_dir)
    _compile(cfg.proto_dir, cfg.cpp.proto_output_dir, "cpp_out", cfg)

    # Sub-directories (tip, operator)
    for sub in ("tip", "operator"):
        src = cfg.proto_dir / sub
        if src.exists():
            dst = cfg.cpp.proto_output_dir / sub
            ensure_dirs(dst)
            _compile(src, dst, "cpp_out", cfg)


def compile_proto_go(cfg: ExporterConfig) -> None:
    """Compile all ``.proto`` → Go using *protoc*."""
    if not cfg.go.enabled:
        return
    ensure_dirs(cfg.go.proto_output_dir)
    _compile(cfg.proto_dir, cfg.go.proto_output_dir, "go_out", cfg)

    for sub in ("tip", "operator"):
        src = cfg.proto_dir / sub
        if src.exists():
            _compile(src, cfg.go.proto_output_dir, "go_out", cfg)


def compile_proto_java(cfg: ExporterConfig) -> None:
    """Compile all ``.proto`` → Java using *protoc*."""
    if not cfg.java.enabled:
        return
    ensure_dirs(cfg.java.proto_output_dir)
    _compile(cfg.proto_dir, cfg.java.proto_output_dir, "java_out", cfg)

    for sub in ("tip", "operator"):
        src = cfg.proto_dir / sub
        if src.exists():
            dst = cfg.java.proto_output_dir / sub
            ensure_dirs(dst)
            _compile(src, dst, "java_out", cfg)


def _compile(source_dir: Path, output_dir: Path, out_flag: str, cfg: ExporterConfig) -> None:
    """Run *protoc* on all ``.proto`` files in *source_dir*."""
    proto_files = _collect_protos(source_dir)
    if not proto_files:
        logger.warning("No .proto files in %s", source_dir)
        return

    cmd = [cfg.protoc_command, f"--proto_path={source_dir.resolve()}"]
    for inc in cfg.protoc_extra_includes:
        cmd.append(f"--proto_path={inc}")
    cmd.append(f"--proto_path={cfg.proto_dir.resolve()}")
    cmd.append(f"--{out_flag}={output_dir.resolve()}")

    if out_flag == "go_out":
        cmd.append(f"--go-grpc_out={output_dir.resolve()}")

    cmd.extend(proto_files)

    try:
        result = subprocess.run(
            cmd, check=True, capture_output=True, text=True, cwd=str(source_dir)
        )
        if result.stderr:
            logger.info("protoc stderr: %s", result.stderr.strip())
        logger.info("Compiled %d proto files → %s", len(proto_files), output_dir)
    except subprocess.CalledProcessError as exc:
        logger.error("protoc failed in %s: %s", source_dir, exc.stderr)


def _collect_protos(source_dir: Path) -> list[str]:
    protos: list[str] = []
    for dirpath, _, filenames in os.walk(source_dir):
        for fn in filenames:
            if fn.endswith(".proto"):
                abs_f = Path(dirpath) / fn
                protos.append(abs_f.relative_to(source_dir).as_posix())
    return protos
