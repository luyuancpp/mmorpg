-- 游戏交付扩展：仅创建新的可靠合同表，绝不修改已经支付订单的事实。
CREATE TABLE IF NOT EXISTS commerce_sku_grant (sku_id BIGINT PRIMARY KEY,item_config_id INT NOT NULL,count_per_unit INT NOT NULL,title VARCHAR(128) NOT NULL,CHECK (item_config_id>0),CHECK (count_per_unit>0 AND count_per_unit<=4096));
CREATE TABLE IF NOT EXISTS commerce_order_grant (order_sn VARCHAR(36) PRIMARY KEY,item_config_id INT NOT NULL,count_per_unit INT NOT NULL,title VARCHAR(128) NOT NULL);
CREATE TABLE IF NOT EXISTS commerce_delivery_receipt (order_sn VARCHAR(36) PRIMARY KEY,trade_order_id VARCHAR(20) NOT NULL,state VARCHAR(24) NOT NULL,proof VARCHAR(64) NOT NULL);
CREATE TABLE IF NOT EXISTS commerce_refund_intent (order_sn VARCHAR(36) PRIMARY KEY,status VARCHAR(24) NOT NULL,channel VARCHAR(32) NULL,transaction_id VARCHAR(128) NULL);
CREATE TABLE IF NOT EXISTS commerce_refund_receipt (order_sn VARCHAR(36) PRIMARY KEY,channel VARCHAR(32) NOT NULL,transaction_id VARCHAR(128) NOT NULL,amount_minor BIGINT NOT NULL,currency VARCHAR(16) NOT NULL,UNIQUE(channel,transaction_id));
-- 此语句只用于旧v1升级；全新安装的schema.sql已经创建该索引，不再执行本文件。
CREATE INDEX idx_commerce_delivery_pending ON commerce_delivery_intent(status,created_ms,order_sn);
