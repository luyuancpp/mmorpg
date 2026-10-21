package com.game.mall;

import java.nio.charset.StandardCharsets;
import java.time.Clock;
import java.util.List;
import java.util.Objects;
import java.util.UUID;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.function.LongSupplier;
import java.util.function.Supplier;
import org.springframework.beans.factory.ObjectProvider;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.context.annotation.DependsOn;
import org.springframework.dao.DuplicateKeyException;
import org.springframework.jdbc.core.JdbcTemplate;
import org.springframework.stereotype.Component;
import org.springframework.transaction.PlatformTransactionManager;
import org.springframework.transaction.TransactionDefinition;
import org.springframework.transaction.support.TransactionTemplate;

/** 本地出款提供方。先独立提交原件，再驱动结算；未知结果始终重放原单和原流水。 */
@Component
@DependsOn("commerceSchemaReady")
public final class MockPayoutStore implements RmbConsignmentStore.PayoutEvidenceVerifier {
    public record Pay(String withdrawalId, String requestId) {}
    private record Fact(String withdrawalId, byte[] request, String transactionId, long amount, boolean successful) {
        RmbConsignmentApi.WithdrawalReceipt receipt() {
            return new RmbConsignmentApi.WithdrawalReceipt(withdrawalId, "mock", transactionId, amount, "CNY", successful);
        }
    }
    private final JdbcTemplate jdbc;
    private final PlatformTransactionManager manager;
    private final CommerceProperties props;
    private final ObjectProvider<RmbConsignmentStore> settlements;
    private final Clock clock;
    private final LongSupplier nanoTime;
    private final AtomicBoolean scanning = new AtomicBoolean();
    // 扫描状态只由scanning持有者访问；重启从头扫描，SQL原件保证幂等。
    private String after = "", upper = "";
    @Autowired public MockPayoutStore(JdbcTemplate jdbc, PlatformTransactionManager manager, CommerceProperties props,
            ObjectProvider<RmbConsignmentStore> settlements, Clock clock) {
        this(jdbc, manager, props, settlements, clock, System::nanoTime);
    }
    MockPayoutStore(JdbcTemplate jdbc, PlatformTransactionManager manager, CommerceProperties props,
            ObjectProvider<RmbConsignmentStore> settlements, Clock clock, LongSupplier nanoTime) {
        this.jdbc = new JdbcTemplate(Objects.requireNonNull(jdbc.getDataSource()));
        this.jdbc.setQueryTimeout(5);
        this.manager = manager; this.props = props; this.settlements = settlements; this.clock = clock; this.nanoTime = nanoTime;
    }
    public RmbConsignmentApi.Withdrawal pay(Pay input) {
        props.requireMockPayment();
        if (input == null || input.withdrawalId() == null || !input.withdrawalId().matches(
                "[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}")) {
            throw new CommerceException(400, "mock提现编号非法");
        }
        RmbConsignmentStore.request(input.requestId());
        var budget = new Budget(15);
        Fact fact;
        try {
            fact = transaction(budget, () -> {
                // 原提现锁串行化两种requestId并发，不接受调用方金额或成功声明。
                var values = jdbc.queryForList("SELECT amount_fen,state FROM commerce_withdrawal WHERE withdrawal_id=? FOR UPDATE", input.withdrawalId());
                if (values.isEmpty()) { throw new CommerceException(404, "提现单不存在"); }
                Fact prior = fact(jdbc, input.withdrawalId());
                if (prior != null) { sameRequest(prior, input); return prior; }
                if (!"REQUESTED".equals(values.getFirst().get("state"))) { throw new CommerceException(409, "提现单不在待出款状态"); }
                long amount = ((Number) values.getFirst().get("amount_fen")).longValue();
                if (amount < 1 || amount > CommerceStore.MAX_AMOUNT_MINOR) { throw new CommerceException(503, "提现金额原件损坏"); }
                jdbc.update("INSERT INTO commerce_mock_payout(withdrawal_id,request_id,transaction_id,amount_fen,successful,created_ms) "
                    + "VALUES(?,?,?,?,1,?)", input.withdrawalId(), bytes(input.requestId()), bytes("mock-payout-" + UUID.randomUUID()), amount, clock.millis());
                return Objects.requireNonNull(fact(jdbc, input.withdrawalId()));
            });
        } catch (DuplicateKeyException duplicate) { throw new CommerceException(409, "mock出款请求号已用于其它提现"); }
        return apply(fact, budget);
    }
    private RmbConsignmentApi.Withdrawal apply(Fact fact, Budget budget) {
        // 结算的REQUIRED事务加入此有剩余预算的边界，不另开无关的完整5秒窗口。
        var result = transaction(budget, () -> settlements.getObject().completeWithdrawal(fact.receipt()));
        transaction(budget, () -> {
            jdbc.update("UPDATE commerce_mock_payout SET applied=1 WHERE withdrawal_id=? AND transaction_id=? AND applied=0",
                fact.withdrawalId(), bytes(fact.transactionId()));
            Integer applied = jdbc.queryForObject("SELECT applied FROM commerce_mock_payout WHERE withdrawal_id=? AND transaction_id=?",
                Integer.class, fact.withdrawalId(), bytes(fact.transactionId()));
            if (applied == null || applied != 1) { throw new CommerceException(503, "mock出款投递标记原件不符"); }
            return Boolean.TRUE;
        });
        return result;
    }
    @Override public void verify(JdbcTemplate transaction, RmbConsignmentApi.WithdrawalReceipt input) {
        props.requireMockPayment();
        Fact original = fact(transaction, input.withdrawalId());
        if (original == null || !original.receipt().equals(input)) {
            throw new CommerceException(409, "mock出款必须匹配适配器持久原件");
        }
    }
    public MockPaymentApi.Reconcile reconcile() {
        props.requireMockPayment();
        if (!scanning.compareAndSet(false, true)) { return new MockPaymentApi.Reconcile(0, 0, 0, 0, true, false); }
        int attempted = 0, succeeded = 0, failed = 0, scanFailures = 0;
        boolean exhausted = false;
        var budget = new Budget(3);
        try {
            while (attempted < 40) {
                if (!budget.canStart()) { exhausted = true; break; }
                List<String> ids;
                try {
                    if (upper.isEmpty()) {
                        var values = transaction(budget, () -> jdbc.queryForList("SELECT withdrawal_id FROM commerce_mock_payout WHERE applied=0 ORDER BY withdrawal_id DESC LIMIT 1", String.class));
                        if (values.isEmpty()) { break; }
                        upper = values.getFirst();
                    }
                    ids = transaction(budget, () -> jdbc.queryForList("SELECT withdrawal_id FROM commerce_mock_payout WHERE applied=0 "
                        + "AND withdrawal_id>? AND withdrawal_id<=? ORDER BY withdrawal_id LIMIT 1", String.class, after, upper));
                    if (ids.isEmpty()) { after = ""; upper = ""; break; }
                } catch (RuntimeException unavailable) { scanFailures++; break; }
                if (!budget.canStart()) { exhausted = true; break; }
                String id = ids.getFirst();
                attempted++;
                try { apply(transaction(budget, () -> Objects.requireNonNull(fact(jdbc, id))), budget); succeeded++; }
                catch (RuntimeException unavailable) { failed++; }
                after = id;
            }
            return new MockPaymentApi.Reconcile(attempted, succeeded, failed, scanFailures, false, exhausted);
        } finally { scanning.set(false); }
    }
    private static Fact fact(JdbcTemplate transaction, String id) {
        var rows = transaction.query("SELECT * FROM commerce_mock_payout WHERE withdrawal_id=?", (row, index) -> {
            String transactionId = new String(row.getBytes("transaction_id"), StandardCharsets.US_ASCII);
            long amount = row.getLong("amount_fen"); int successful = row.getInt("successful");
            if (!transactionId.matches("mock-payout-[0-9a-f-]{36}") || amount < 1 || amount > CommerceStore.MAX_AMOUNT_MINOR
                    || successful < 0 || successful > 1) { throw new CommerceException(503, "mock出款原件损坏"); }
            return new Fact(row.getString("withdrawal_id"), row.getBytes("request_id"), transactionId, amount, successful == 1);
        }, id);
        return rows.isEmpty() ? null : rows.getFirst();
    }
    private static void sameRequest(Fact fact, Pay input) {
        if (!java.util.Arrays.equals(fact.request(), bytes(input.requestId()))) { throw new CommerceException(409, "同一提现不能重建渠道出款请求"); }
    }
    private <T> T transaction(Budget budget, Supplier<T> action) {
        if (!budget.canStart()) { throw new CommerceException(503, "mock出款预算已用尽，原件保留待恢复"); }
        var boundary = new TransactionTemplate(manager);
        boundary.setPropagationBehavior(TransactionDefinition.PROPAGATION_REQUIRES_NEW);
        boundary.setIsolationLevel(TransactionDefinition.ISOLATION_READ_COMMITTED);
        boundary.setTimeout((int) Math.max(1, Math.min(5, (budget.remaining() + 999_999_999L) / 1_000_000_000L)));
        return Objects.requireNonNull(boundary.execute(status -> action.get()));
    }
    private final class Budget {
        private final long start = nanoTime.getAsLong(), duration;
        Budget(int seconds) { duration = seconds * 1_000_000_000L; }
        long remaining() { return duration - (nanoTime.getAsLong() - start); }
        boolean canStart() { return remaining() >= 250_000_000L; }
    }
    private static byte[] bytes(String value) { return value.getBytes(StandardCharsets.US_ASCII); }
}
