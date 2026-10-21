package com.game.mall;

/** 待办扫描固定本轮上界；调用方只在实际尝试后保存对应位置，尾页空串从头绕回。 */
record PendingScanCursor(long afterMs, String afterId, long upperMs, String upperId) {
    private static final String UUID = "[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}";
    String after(long createdMs, String orderId) {
        return "v1:" + createdMs + ":" + orderId + ":" + upperMs + ":" + upperId;
    }
    static PendingScanCursor parse(String raw) {
        if (raw == null || raw.isEmpty()) { return null; }
        if (raw.length() > 256) { throw invalid(); }
        String[] fields = raw.split(":", -1);
        if (fields.length != 5 || !fields[0].equals("v1") || !fields[1].matches("0|[1-9][0-9]*")
            || !fields[3].matches("0|[1-9][0-9]*") || !fields[2].matches(UUID) || !fields[4].matches(UUID)) { throw invalid(); }
        try {
            long after = Long.parseLong(fields[1]), upper = Long.parseLong(fields[3]);
            if (after > upper || after == upper && fields[2].compareTo(fields[4]) > 0) { throw invalid(); }
            return new PendingScanCursor(after, fields[2], upper, fields[4]);
        } catch (NumberFormatException invalid) { throw invalid(); }
    }
    private static CommerceException invalid() { return new CommerceException(400, "交付游标非法或越过扫描上界"); }
}
