import os
import json

# 定义临时文件路径
temp_file_path = 'id_pool.json'

def read_temp_id_mapping():
    """读取临时ID映射文件"""
    if os.path.exists(temp_file_path):
        with open(temp_file_path, 'r') as f:
            return json.load(f)
    else:
        return {}

def write_temp_id_mapping(mapping):
    """写入临时ID映射文件"""
    with open(temp_file_path, 'w', encoding='utf-8') as f:
        json.dump(mapping, f, indent=2)

def allocate_new_id(mapping):
    """分配新的ID"""
    if not mapping:
        return 1
    else:
        max_id = max(mapping.values())
        return max_id + 1

def update_id_mapping(field_name, field_id, mapping):
    """更新ID映射"""
    mapping[field_name] = field_id
    write_temp_id_mapping(mapping)

def generate_proto_file(field_name):
    """生成Proto文件"""
    mapping = read_temp_id_mapping()

    if field_name in mapping:
        field_id = mapping[field_name]
    else:
        field_id = allocate_new_id(mapping)
        update_id_mapping(field_name, field_id, mapping)

    # 在此处生成Proto文件，使用field_name和field_id

# 示例用法
generate_proto_file("field1")
generate_proto_file("field2")
generate_proto_file("field3")

