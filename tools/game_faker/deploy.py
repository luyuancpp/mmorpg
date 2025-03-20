import mysql.connector
import random
import logging

# 配置日志记录
logging.basicConfig(level=logging.INFO)


# 连接到 MySQL 数据库
def connect_to_db():
    try:
        return mysql.connector.connect(
            host='localhost',
            user='root',
            password='root',
            database='deploy'  # 修改为你的数据库名称 "deploy"
        )
    except mysql.connector.Error as err:
        logging.error(f"Error: {err}")
        raise


# 清空表中的数据
def clear_table(cursor, table_name):
    try:
        truncate_query = f"TRUNCATE TABLE {table_name}"
        cursor.execute(truncate_query)
        logging.info(f"Table {table_name} has been cleared.")
    except mysql.connector.Error as err:
        logging.error(f"Error clearing table {table_name}: {err}")
        raise


# 初始化端口管理
def initialize_ports():
    return {
        'gate_node_db': 2000,
        'centre_node_db': 3000,
        'login_node_db': 4000,
        'game_node_db': 6000,
        'database_node_db': 10000,
    }


# 生成模拟数据
def generate_simulated_data(index, table_name, used_ports, port_counter, zone_id_counter, zone_id_step):
    ip = "127.0.0.1"

    # 获取当前表的 zone_id，并且每生成 `zone_id_step` 个节点后递增一次
    zone_id = zone_id_counter[table_name]
    if index > 0 and index % zone_id_step == 0:
        zone_id_counter[table_name] += 1  # 达到步长时，递增 zone_id

    # 获取当前表的起始端口
    current_port = port_counter[table_name]

    # 检查端口是否已经使用（用于防止端口重复）
    if current_port in used_ports:
        # 如果端口已用，递增端口
        port_counter[table_name] += 1
        current_port = port_counter[table_name]

    used_ports.add(current_port)

    # 对于 login_node_db 和 database_node_db，返回 addr 格式 "127.0.0.1:port"
    if table_name == 'login_node_db' or table_name == 'database_node_db':
        addr = f"{ip}:{current_port}"
        return addr, None, zone_id

    # 对其他表，返回 ip, port 和 zone_id
    return ip, current_port, zone_id


# 插入数据到表
def insert_data_to_table(cursor, table_name, used_ports, port_counter, zone_id_counter, total_nodes, zone_id_step):
    data_to_insert = []
    # 根据每个表需要插入的节点数进行插入
    for i in range(total_nodes):  # 动态生成节点数
        if table_name == 'login_node_db' or table_name == 'database_node_db':
            addr, _, zone_id = generate_simulated_data(i, table_name, used_ports, port_counter, zone_id_counter, zone_id_step)
            data_to_insert.append((addr, zone_id))
        else:
            ip, port, zone_id = generate_simulated_data(i, table_name, used_ports, port_counter, zone_id_counter, zone_id_step)
            data_to_insert.append((ip, port, zone_id))

    try:
        # 批量插入数据
        if table_name == 'login_node_db' or table_name == 'database_node_db':
            insert_query = f"INSERT INTO {table_name} (addr, zone_id) VALUES (%s, %s)"
        else:
            insert_query = f"INSERT INTO {table_name} (ip, port, zone_id) VALUES (%s, %s, %s)"

        cursor.executemany(insert_query, data_to_insert)
        logging.info(f"Inserted {len(data_to_insert)} records into {table_name}")
    except mysql.connector.Error as err:
        logging.error(f"Error inserting data into {table_name}: {err}")
        raise


# 主函数
def main():
    # 创建数据库连接
    conn = connect_to_db()
    cursor = conn.cursor()

    # 定义表名列表
    tables = ['centre_node_db', 'game_node_db', 'gate_node_db', 'login_node_db', 'database_node_db']

    # 用于追踪已使用的端口
    used_ports = set()

    # 初始化端口管理
    port_counter = initialize_ports()

    # 配置每个表需要插入的节点数量（现在表示总数）
    total_nodes = {
        'login_node_db': 30,  # 总共插入 30 个 login_node_db 节点
        'gate_node_db': 50,  # 总共插入 50 个 gate_node_db 节点
        'game_node_db': 120,  # 总共插入 120 个 game_node_db 节点
        'centre_node_db': 12,  # 总共插入 12 个 centre_node_db 节点
        'database_node_db': 12  # 总共插入 12 个 database_node_db 节点
    }

    # 初始化 zone_id 自增
    zone_id_counter = {
        'gate_node_db': 1,  # gate_node_db 从 zone_id 1 开始
        'game_node_db': 1,  # game_node_db 从 zone_id 1 开始
        'centre_node_db': 1,  # centre_node_db 从 zone_id 1 开始
        'login_node_db': 1,  # login_node_db 从 zone_id 1 开始
        'database_node_db': 1  # database_node_db 从 zone_id 1 开始
    }

    # 定义每个表的 zone_id 步长
    zone_id_steps = {
        'gate_node_db': 12,  # 每 12 个节点递增一次 zone_id
        'game_node_db': 12,  # 每 12 个节点递增一次 zone_id
        'centre_node_db': 12,  # 每 12 个节点递增一次 zone_id
        'login_node_db': 10,  # 每 10 个节点递增一次 zone_id
        'database_node_db': 10  # 每 10 个节点递增一次 zone_id
    }

    # 清空并填充数据到每个表
    for table in tables:
        clear_table(cursor, table)  # 清空表数据
        insert_data_to_table(cursor, table, used_ports, port_counter, zone_id_counter, total_nodes[table], zone_id_steps[table])  # 插入模拟数据

    # 提交事务
    conn.commit()

    # 关闭游标和连接
    cursor.close()
    conn.close()


if __name__ == "__main__":
    main()
