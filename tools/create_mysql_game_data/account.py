import mysql.connector

# 连接到 MySQL 数据库
conn = mysql.connector.connect(
    host='localhost',
    user='root',
    password='root',
    database='game'
)

cursor = conn.cursor()

# 批量插入 account 数据
for i in range(10000):  # 假设插入 luhailong0 到 luhailong100
    account_name = f"luhailong{i}"
    insert_query = """
    INSERT INTO user_accounts (account)
    SELECT %s WHERE NOT EXISTS (SELECT 1 FROM user_accounts WHERE account = %s)
    """
    cursor.execute(insert_query, (account_name, account_name))

conn.commit()
cursor.close()
conn.close()
