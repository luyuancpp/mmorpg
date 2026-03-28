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


# --- Component helpers (for per-column ECS component generation) ---

_CPP_COMP_TYPE_MAP: dict[str, str] = {
    "int32":  "int32_t",
    "int64":  "int64_t",
    "uint32": "uint32_t",
    "uint64": "uint64_t",
    "float":  "float",
    "double": "double",
    "bool":   "bool",
    "string": "std::string_view",
}


def to_cpp_comp_type(proto_type: str) -> str:
    """C++ type for a per-column ECS component value (string → string_view)."""
    return _CPP_COMP_TYPE_MAP.get(proto_type, proto_type)


def to_cpp_repeated_elem_type(proto_type: str) -> str:
    """C++ element type inside a proto RepeatedField (for span)."""
    return _CPP_TYPE_MAP.get(proto_type, proto_type)


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


def to_go_proto_field(snake_name: str) -> str:
    """Convert snake_case proto field name to Go CamelCase struct field.

    Example: ``sub_buff`` → ``SubBuff``, ``id`` → ``Id``.
    """
    return "".join(word.capitalize() for word in snake_name.split("_"))


def to_go_repeated_elem_type(proto_type: str) -> str:
    """Go element type for repeated fields (same as to_go_type)."""
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


def to_java_proto_getter(snake_name: str) -> str:
    """Convert snake_case field name to Java proto getter (e.g. ``sub_buff`` → ``getSubBuff``)."""
    camel = "".join(word.capitalize() for word in snake_name.split("_"))
    return f"get{camel}"


def to_java_repeated_elem_type(proto_type: str) -> str:
    """Java element type inside a proto repeated field (boxed for generics)."""
    return _JAVA_BOXED_MAP.get(proto_type, proto_type)
