"""Language-specific type conversions.

Centralises the mapping from proto/Excel types to C++, Go, and Java types.
"""

from __future__ import annotations

# ---------------------------------------------------------------------------
# C++
# ---------------------------------------------------------------------------

_CPP_TYPE_MAP: dict[str, str] = {
    "int32":  "int32_t",
    "int64":  "int64_t",
    "uint32": "uint32_t",
    "uint64": "uint64_t",
    "float":  "float",
    "double": "double",
    "bool":   "bool",
    "string": "std::string",
}


def to_cpp_type(proto_type: str) -> str:
    return _CPP_TYPE_MAP.get(proto_type, proto_type)


def to_cpp_param_type(proto_type: str) -> str:
    """C++ function parameter type (const ref for strings)."""
    if proto_type == "string":
        return "const std::string&"
    return to_cpp_type(proto_type)


def cpp_map_type(is_multi: bool) -> str:
    return "unordered_multimap" if is_multi else "unordered_map"


# ---------------------------------------------------------------------------
# Go
# ---------------------------------------------------------------------------

_GO_TYPE_MAP: dict[str, str] = {
    "int32":  "int32",
    "int64":  "int64",
    "uint32": "uint32",
    "uint64": "uint64",
    "float":  "float32",
    "double": "float64",
    "bool":   "bool",
    "string": "string",
}


def to_go_type(proto_type: str) -> str:
    return _GO_TYPE_MAP.get(proto_type, "interface{}")


# ---------------------------------------------------------------------------
# Java
# ---------------------------------------------------------------------------

_JAVA_TYPE_MAP: dict[str, str] = {
    "int32":  "int",
    "int64":  "long",
    "uint32": "int",
    "uint64": "long",
    "float":  "float",
    "double": "double",
    "bool":   "boolean",
    "string": "String",
}

_JAVA_BOXED_MAP: dict[str, str] = {
    "int32":  "Integer",
    "int64":  "Long",
    "uint32": "Integer",
    "uint64": "Long",
    "float":  "Float",
    "double": "Double",
    "bool":   "Boolean",
    "string": "String",
}


def to_java_type(proto_type: str) -> str:
    return _JAVA_TYPE_MAP.get(proto_type, proto_type)


def to_java_boxed(proto_type: str) -> str:
    return _JAVA_BOXED_MAP.get(proto_type, proto_type)
