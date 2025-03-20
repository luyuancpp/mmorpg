import mysql.connector
import random


# 连接到 MySQL 数据库
def connect_to_db():
    return mysql.connector.connect(
        host='localhost',
        user='root',
        password='root',
        database='deploy'  # 修改为你的数据库名称
    )


# 清空表中的数据
def clear_table(cursor, table_name):
    truncate_query = f"TRUNCATE TABLE {table_name}"
    cursor.execute(truncate_query)


# 生成模拟数据
def generate_simulated_data(index, table_name):
    # 将 IP 地址固定为 127.0.0.1
    ip = "127.0.0.1"

    # 针对 gate_node_db 表，端口使用固定的值
    if table_name == 'gate_node_db':
        port = 20000  # 假设 gate_node_db 使用端口 20000，你可以修改为任何固定的端口
    else:
        # 对其他表，端口在 10000 到 60000 之间随机生成
        port = random.randint(10000, 60000)

    # 生成 zone_id （假设 zone_id 是从 1 到 100）
    zone_id = random.randint(1, 100)

    return (ip, port, zone_id)


# 插入数据到表
def insert_data_to_table(cursor, table_name):
    for i in range(10):  # 这里插入 10 行数据，你可以根据需要修改数量
        ip, port, zone_id = generate_simulated_data(i, table_name)

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

    # 清空并填充数据到每个表
    for table in tables:
        clear_table(cursor, table)  # 清空表数据
        insert_data_to_table(cursor, table)  # 插入模拟数据

    # 提交事务
    conn.commit()

    # 关闭游标和连接
    cursor.close()
    conn.close()


if __name__ == "__main__":
    main()
