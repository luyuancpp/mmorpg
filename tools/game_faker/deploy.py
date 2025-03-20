import mysql.connector
import random


# 连接到 MySQL 数据库
def connect_to_db():
    return mysql.connector.connect(
        host='localhost',
        user='root',
        password='root',
        database='deploy'  # 修改为你的数据库名称 "deploy"
    )


# 清空表中的数据
def clear_table(cursor, table_name):
    truncate_query = f"TRUNCATE TABLE {table_name}"
    cursor.execute(truncate_query)
    print(f"Table {table_name} has been cleared.")


# 初始化端口管理
def initialize_ports():
    return {
        'gate_node_db': 20000,
        'centre_node_db': 30000,
        'login_node_db': 40000,
        'game_node_db': 60000,
    }


# 生成模拟数据
def generate_simulated_data(index, table_name, used_ports, port_counter, zone_id_counter):
    ip = "127.0.0.1"

    # 对于 centre_node_db 表，zone_id 固定，唯一递增
    if table_name == 'centre_node_db':
        zone_id = zone_id_counter['centre_node_db']
        zone_id_counter['centre_node_db'] += 1  # centre 的 zone_id 递增
    else:
        # 对于其他表，zone_id 每组递增
        zone_id = zone_id_counter[table_name]
        if index == 0:  # 如果是每组的第一个节点，递增 zone_id
            zone_id_counter[table_name] += 1

    # 获取当前表的起始端口
    current_port = port_counter[table_name]

    # 检查端口是否已经使用（用于防止端口重复）
    if current_port in used_ports:
        # 如果端口已用，递增端口
        port_counter[table_name] += 1
        current_port = port_counter[table_name]

    used_ports.add(current_port)

    # 对于 login_node_db，返回 addr 格式 "127.0.0.1:port"
    if table_name == 'login_node_db':
        addr = f"{ip}:{current_port}"
        return addr, None, zone_id

    # 对其他表，返回 ip, port 和 zone_id
    return ip, current_port, zone_id


# 插入数据到表
def insert_data_to_table(cursor, table_name, used_ports, port_counter, zone_id_counter, nodes_per_group):
    # 根据每个表需要插入的节点数进行插入
    for i in range(nodes_per_group):  # 动态生成节点数
        if table_name == 'login_node_db':
            addr, _, zone_id = generate_simulated_data(i, table_name, used_ports, port_counter, zone_id_counter)
            insert_query = f"""
            INSERT INTO {table_name} (addr, zone_id)
            VALUES (%s, %s)
            """
            cursor.execute(insert_query, (addr, zone_id))
        else:
            ip, port, zone_id = generate_simulated_data(i, table_name, used_ports, port_counter, zone_id_counter)
            insert_query = f"""
            INSERT INTO {table_name} (ip, port, zone_id)
            VALUES (%s, %s, %s)
            """
            cursor.execute(insert_query, (ip, port, zone_id))


# 主函数
def main():
    # 创建数据库连接
    conn = connect_to_db()
    cursor = conn.cursor()

    # 定义表名列表
    tables = ['centre_node_db', 'game_node_db', 'gate_node_db', 'login_node_db']

    # 用于追踪已使用的端口
    used_ports = set()

    # 初始化端口管理
    port_counter = initialize_ports()

    # 配置每个表需要插入的节点数量
    nodes_per_group = {
        'login_node_db': 3,  # 每个 login_node_db 插入 3 个节点
        'gate_node_db': 5,  # 每个 gate_node_db 插入 5 个节点
        'game_node_db': 12,  # 每个 game_node_db 插入 12 个节点
        'centre_node_db': 12  # centre_node_db 插入 12 个节点
    }

    # 初始化 zone_id 自增
    zone_id_counter = {
        'gate_node_db': 1,  # gate_node_db 从 zone_id 1 开始
        'game_node_db': 1,  # game_node_db 从 zone_id 1 开始
        'centre_node_db': 1,  # centre_node_db 从 zone_id 1 开始
        'login_node_db': 1  # login_node_db 从 zone_id 1 开始
    }

    # 清空并填充数据到每个表
    for table in tables:
        clear_table(cursor, table)  # 清空表数据
        insert_data_to_table(cursor, table, used_ports, port_counter, zone_id_counter, nodes_per_group[table])  # 插入模拟数据

    # 提交事务
    conn.commit()

    # 关闭游标和连接
    cursor.close()
    conn.close()


if __name__ == "__main__":
    main()
