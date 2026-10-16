-- 从 macrozheng/mall dcaa93b3 的 PMS/OMS 提取并加固；只新增，不 DROP、不写样例账户。
-- 第三方电商表例外：本模块 SQL 是迁移事实源，不属于游戏 proto 表生成树。
-- price_minor/pay_amount 全部为最小货币单位整数；player_id 使用规范 uint64 十进制字符串。
-- 必须使用独立库，不能连接上游 mall 实物电商库；此事实源只负责全新 v1 安装。
CREATE TABLE IF NOT EXISTS commerce_schema_version (
  version INT PRIMARY KEY,
  storage_contract VARCHAR(80) NOT NULL
);
CREATE TABLE IF NOT EXISTS commerce_buyer_guard (buyer_id VARCHAR(20) PRIMARY KEY);
CREATE TABLE IF NOT EXISTS pms_sku_stock (
  id BIGINT PRIMARY KEY,
  product_id BIGINT NOT NULL,
  sku_code VARCHAR(64) NOT NULL,
  price_minor BIGINT NOT NULL,
  currency VARCHAR(16) NOT NULL,
  stock INT NOT NULL,
  lock_stock INT NOT NULL DEFAULT 0,
  published INT NOT NULL DEFAULT 0,
  CHECK (price_minor > 0), CHECK (stock >= 0), CHECK (lock_stock >= 0 AND lock_stock <= stock)
);
CREATE TABLE IF NOT EXISTS oms_order (
  id BIGINT AUTO_INCREMENT PRIMARY KEY,
  order_sn VARCHAR(36) NOT NULL,
  buyer_id VARCHAR(20) NOT NULL,
  request_id VARBINARY(128) NOT NULL,
  sku_id BIGINT NOT NULL,
  quantity INT NOT NULL,
  pay_amount BIGINT NOT NULL,
  currency VARCHAR(16) NOT NULL,
  status INT NOT NULL,
  create_time_ms BIGINT NOT NULL,
  payment_deadline_ms BIGINT NOT NULL,
  payment_time_ms BIGINT NULL,
  UNIQUE (order_sn), UNIQUE (buyer_id, request_id), INDEX idx_commerce_buyer_status (buyer_id,status),
  CHECK (quantity > 0), CHECK (pay_amount > 0)
);
CREATE TABLE IF NOT EXISTS oms_order_item (
  order_sn VARCHAR(36) PRIMARY KEY,
  product_id BIGINT NOT NULL,
  product_sku_id BIGINT NOT NULL,
  product_sku_code VARCHAR(64) NOT NULL,
  product_quantity INT NOT NULL,
  product_price_minor BIGINT NOT NULL
);
CREATE TABLE IF NOT EXISTS commerce_payment_receipt (
  channel VARBINARY(32) NOT NULL,
  transaction_id VARBINARY(128) NOT NULL,
  order_sn VARCHAR(36) NOT NULL,
  amount_minor BIGINT NOT NULL,
  currency VARCHAR(16) NOT NULL,
  PRIMARY KEY (channel, transaction_id), UNIQUE (order_sn)
);
CREATE TABLE IF NOT EXISTS commerce_delivery_intent (
  order_sn VARCHAR(36) PRIMARY KEY,
  buyer_id VARCHAR(20) NOT NULL,
  sku_id BIGINT NOT NULL,
  quantity INT NOT NULL,
  status VARCHAR(24) NOT NULL,
  created_ms BIGINT NOT NULL
);
CREATE TABLE IF NOT EXISTS commerce_request_nonce (
  caller_role VARCHAR(16) NOT NULL,
  nonce VARBINARY(128) NOT NULL,
  expires_ms BIGINT NOT NULL,
  PRIMARY KEY (caller_role, nonce),
  INDEX idx_commerce_nonce_expiry (expires_ms)
);
-- 安装成功才盖章；应用只读验证，不自行重跑 DDL。此安装脚本仅用于全新独立库。
INSERT INTO commerce_schema_version(version,storage_contract) VALUES (1,'mall-game-official-sku-v1-minor-units');
