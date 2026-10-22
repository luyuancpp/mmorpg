package com.game.mall;

import com.fasterxml.jackson.core.JsonParser;
import com.fasterxml.jackson.databind.DeserializationFeature;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.cfg.CoercionAction;
import com.fasterxml.jackson.databind.cfg.CoercionInputShape;
import com.fasterxml.jackson.databind.type.LogicalType;
import jakarta.servlet.http.HttpServletRequest;
import java.io.IOException;
import java.util.List;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RestController;

/** 账号由已认证trade从登录上下文取得；客户端不能直接声明账号或资产证明。 */
@RestController
public final class RmbConsignmentController {
    private final RmbConsignmentStore consignments;
    private final CommerceStore orders;
    private final ObjectMapper json;
    public RmbConsignmentController(RmbConsignmentStore consignments, CommerceStore orders, ObjectMapper json) {
        this.consignments = consignments; this.orders = orders; this.json = json.copy();
        this.json.coercionConfigFor(LogicalType.Textual)
            .setCoercion(CoercionInputShape.Integer, CoercionAction.Fail)
            .setCoercion(CoercionInputShape.Float, CoercionAction.Fail)
            .setCoercion(CoercionInputShape.Boolean, CoercionAction.Fail);
    }
    @PostMapping("/internal/consignments/register")
    public RmbConsignmentApi.Listing register(HttpServletRequest request) {
        return consignments.register(input(request, RmbConsignmentApi.Escrow.class));
    }
    @PostMapping("/internal/consignments/list")
    public RmbConsignmentApi.Listings list(HttpServletRequest request) {
        return consignments.list(input(request, RmbConsignmentApi.Page.class));
    }
    @PostMapping("/internal/consignments/get")
    public RmbConsignmentApi.Listing get(HttpServletRequest request) {
        return consignments.get(input(request, RmbConsignmentApi.Get.class));
    }
    @PostMapping("/internal/consignments/cancel")
    public RmbConsignmentApi.Listing cancel(HttpServletRequest request) {
        return consignments.cancel(input(request, RmbConsignmentApi.Cancel.class));
    }
    @PostMapping("/internal/consignments/buy")
    public CommerceApi.Order buy(HttpServletRequest request) {
        return orders.createConsignment(input(request, RmbConsignmentApi.Buy.class));
    }
    @PostMapping("/internal/consignments/orders/get")
    public CommerceApi.Order order(HttpServletRequest request) {
        return orders.getConsignment(input(request, RmbConsignmentApi.OrderQuery.class));
    }
    @PostMapping("/internal/consignments/orders/list")
    public List<CommerceApi.Order> orders(HttpServletRequest request) {
        return orders.listConsignments(input(request, RmbConsignmentApi.OrderPage.class));
    }
    @PostMapping("/internal/consignments/orders/cancel")
    public CommerceApi.Order cancelOrder(HttpServletRequest request) {
        return orders.cancelConsignment(input(request, RmbConsignmentApi.OrderQuery.class));
    }
    @PostMapping("/internal/consignments/deliveries/poll")
    public RmbConsignmentApi.Deliveries deliveries(HttpServletRequest request) {
        return consignments.deliveries(input(request, DeliveryApi.Poll.class));
    }
    @PostMapping("/internal/consignments/returns/poll")
    public RmbConsignmentApi.Returns returns(HttpServletRequest request) {
        return consignments.returns(input(request, DeliveryApi.Poll.class));
    }
    @PostMapping("/internal/consignments/returns/receipt")
    public void returned(HttpServletRequest request) {
        consignments.returned(input(request, RmbConsignmentApi.ReturnReceipt.class));
    }
    @PostMapping("/internal/consignments/incomes")
    public RmbConsignmentApi.Incomes incomes(HttpServletRequest request) {
        return consignments.incomes(input(request, RmbConsignmentApi.IncomePage.class));
    }
    @PostMapping("/internal/consignments/withdrawals/create")
    public RmbConsignmentApi.Withdrawal withdraw(HttpServletRequest request) {
        return consignments.withdraw(input(request, RmbConsignmentApi.Withdraw.class));
    }
    @PostMapping("/internal/consignments/withdrawals/get")
    public RmbConsignmentApi.Withdrawal withdrawal(HttpServletRequest request) {
        return consignments.getWithdrawal(input(request, RmbConsignmentApi.WithdrawalQuery.class));
    }
    private <T> T input(HttpServletRequest request, Class<T> type) {
        if (!"delivery".equals(request.getAttribute(HmacAuthentication.ROLE))
                || !"trade".equals(request.getAttribute(HmacAuthentication.SUBJECT))) {
            throw new CommerceException(401, "寄售调用方认证失败");
        }
        try {
            return json.readerFor(type).with(DeserializationFeature.FAIL_ON_UNKNOWN_PROPERTIES)
                .with(DeserializationFeature.FAIL_ON_TRAILING_TOKENS).with(JsonParser.Feature.STRICT_DUPLICATE_DETECTION)
                .readValue(request.getInputStream());
        } catch (IOException invalid) { throw new CommerceException(400, "寄售命令必须是完整且唯一字段的JSON对象"); }
    }
}
