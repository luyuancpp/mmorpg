package com.game.mall;
import static org.junit.jupiter.api.Assertions.*;
import org.junit.jupiter.api.Test;
class DeliveryStoreTest {
 @Test void 无游戏映射或交付数量越界下单回滚库存与订单(){try(var f=new CommerceFixture()){f.sku(1,10,100);f.jdbc.update("DELETE FROM commerce_sku_grant WHERE sku_id=1");assertThrows(CommerceException.class,()->f.store.create("1",new CommerceApi.CreateOrder("unknown-sku-001",1,1)));assertEquals(0,f.count("oms_order"));assertEquals(0,f.value("pms_sku_stock","lock_stock"));f.jdbc.update("INSERT INTO commerce_sku_grant(sku_id,item_config_id,count_per_unit,title) VALUES(1,10,4096,'玄铁')");assertThrows(CommerceException.class,()->f.store.create("1",new CommerceApi.CreateOrder("over-sku-001",1,2)));assertEquals(0,f.count("oms_order"));assertEquals(0,f.value("pms_sku_stock","lock_stock"));}}
 @Test void 冻结商品映射_支付后可恢复领取_持久回执幂等且拒改合同(){try(var f=new CommerceFixture()){
  f.sku(1,5,100);f.jdbc.update("UPDATE commerce_sku_grant SET count_per_unit=2 WHERE sku_id=1");
  var order=f.store.create("1",new CommerceApi.CreateOrder("delivery-001",1,2));
  f.jdbc.update("UPDATE commerce_sku_grant SET item_config_id=99,count_per_unit=9 WHERE sku_id=1");
  f.store.recordTrustedPayment(new CommerceApi.Payment(order.orderId(),"test","payment-delivery",order.amountMinor(),order.currency()));
  var store=new DeliveryStore(f.jdbc,f.manager);assertEquals(1,store.poll(20).size());assertEquals(10,store.poll(20).getFirst().itemConfigId());assertEquals(4,store.poll(20).getFirst().itemCount());assertEquals(f.store.get("1",order.orderId()),f.store.list("1",new DeliveryApi.OrderPage("",10)).getFirst());assertTrue(f.store.list("1",new DeliveryApi.OrderPage(order.orderId(),10)).isEmpty());assertThrows(CommerceException.class,()->f.store.list("2",new DeliveryApi.OrderPage(order.orderId(),10)));
  assertEquals("DELIVERY_PENDING",f.store.get("1",order.orderId()).state());
  var receipt=new DeliveryApi.Receipt(order.orderId(),"123","DELIVERED","a".repeat(64));store.receipt(receipt);store.receipt(receipt);
  assertEquals("DELIVERED",f.store.get("1",order.orderId()).state());assertTrue(store.poll(20).isEmpty());
  assertThrows(CommerceException.class,()->store.receipt(new DeliveryApi.Receipt(order.orderId(),"124","DELIVERED","a".repeat(64))));
 }}
 @Test void 拒绝到账才退款_可信渠道退款回执复核金额币种并幂等(){try(var f=new CommerceFixture()){
  f.sku(1,5,100);var order=f.store.create("1",new CommerceApi.CreateOrder("refund-001",1,1));f.store.recordTrustedPayment(new CommerceApi.Payment(order.orderId(),"test","payment-refund",100,"CNY"));
  var store=new DeliveryStore(f.jdbc,f.manager);store.receipt(new DeliveryApi.Receipt(order.orderId(),"123","REFUND_PENDING","b".repeat(64)));assertEquals(1,store.refunds().size());
  assertThrows(CommerceException.class,()->store.refunded(new CommerceApi.Payment(order.orderId(),"test","refund-01",99,"CNY")));
  var refund=new CommerceApi.Payment(order.orderId(),"test","refund-01",100,"CNY");store.refunded(refund);store.refunded(refund);assertEquals("REFUNDED",f.store.get("1",order.orderId()).state());assertTrue(store.refunds().isEmpty());
 }}
}
