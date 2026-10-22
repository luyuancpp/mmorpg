package com.game.mall;

import java.nio.charset.StandardCharsets;
import java.util.Arrays;

/** 账号保持认证原UTF-8字节；角色修订用规范uint64十进制字符串贯穿JSON与SQL。 */
final class CommerceIdentity {
    private CommerceIdentity() {}
    static void account(String value) {
        if (value == null || value.isEmpty() || value.getBytes(StandardCharsets.UTF_8).length > 191
                || !new String(value.getBytes(StandardCharsets.UTF_8), StandardCharsets.UTF_8).equals(value)) {
            throw new CommerceException(400, "订单账号非法");
        }
    }
    static void revision(String value) {
        if (value == null || !value.matches("[1-9][0-9]{0,19}")
                || (value.length() == 20 && value.compareTo("18446744073709551615") > 0)) {
            throw new CommerceException(400, "订单角色归属修订非法");
        }
    }
    static String storedAccount(byte[] value) {
        String text = value == null ? null : new String(value, StandardCharsets.UTF_8);
        if (value == null || !Arrays.equals(value, text.getBytes(StandardCharsets.UTF_8))) {
            throw new CommerceException(503, "订单原账号缺失或损坏");
        }
        try { account(text); }
        catch (CommerceException invalid) { throw new CommerceException(503, "订单原账号缺失或损坏"); }
        return text;
    }
    static String storedRevision(byte[] value) {
        String text = value == null ? null : new String(value, StandardCharsets.US_ASCII);
        try { revision(text); }
        catch (CommerceException invalid) { throw new CommerceException(503, "订单原归属修订缺失或损坏"); }
        return text;
    }
}
