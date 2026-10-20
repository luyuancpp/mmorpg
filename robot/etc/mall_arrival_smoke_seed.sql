-- mall-arrival-smoke 的官方商品种子:**只在本机 dev 商城库执行**(java/mall_game 的独立库,不是游戏库)。
-- 商城没有上架管理接口(java/mall_game 首批只有下单 / 付款 / 发货契约),dev 冒烟只能直接写这两张表。
-- 可重复执行:已存在则把价格 / 发放内容拉回冒烟期望,并把可售库存补到至少 1000000;不碰已有订单与预占。
--
-- 与 robot/etc/mall_arrival_smoke.yaml 对应:sku_id 990001 → 物品 10(可叠加的回血药)× 每份 2。
-- 改这里的发放内容必须同时改 yaml 的 item_config_id / count_per_unit,否则 A9 背包增量对不上。
--
-- 用法(在商城库上):mysql -h127.0.0.1 -P<端口> -u<用户> -p <商城库名> < robot/etc/mall_arrival_smoke_seed.sql

INSERT INTO pms_sku_stock(id, product_id, sku_code, price_minor, currency, stock, lock_stock, published)
VALUES (990001, 990001, 'smoke-mall-arrival-potion-x2', 600, 'CNY', 1000000, 0, 1)
ON DUPLICATE KEY UPDATE price_minor = 600, currency = 'CNY', published = 1,
  stock = GREATEST(stock, lock_stock + 1000000);

INSERT INTO commerce_sku_grant(sku_id, item_config_id, count_per_unit, title)
VALUES (990001, 10, 2, 'Smoke potion x2')
ON DUPLICATE KEY UPDATE item_config_id = 10, count_per_unit = 2, title = 'Smoke potion x2';
