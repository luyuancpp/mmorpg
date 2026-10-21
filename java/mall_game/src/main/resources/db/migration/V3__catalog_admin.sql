-- 仅升级尚无目录管理结构的商城V2库；新安装用schema.sql，不重复执行本文件。
ALTER TABLE pms_sku_stock ADD COLUMN catalog_revision BIGINT NOT NULL DEFAULT 0;
CREATE TABLE commerce_admin_command (
  actor VARBINARY(64) NOT NULL,
  request_id VARBINARY(128) NOT NULL,
  command_type VARCHAR(16) NOT NULL,
  request_digest VARCHAR(64) NOT NULL,
  result_json MEDIUMTEXT NOT NULL,
  created_ms BIGINT NOT NULL,
  PRIMARY KEY(actor,request_id)
);
