# Proto3 Enum Zero Requirement

- In proto3, the first declared enum value must be 0 for open enums.
- If deleting leading enum members like 0/1/2, add a new zero-valued placeholder (for example `Unknown... = 0`) before regenerating, otherwise pbgen/protoc descriptor generation fails.
