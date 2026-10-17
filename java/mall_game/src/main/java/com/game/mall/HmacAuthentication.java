package com.game.mall;

import jakarta.servlet.*;
import jakarta.servlet.http.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.security.*;
import java.util.*;
import javax.crypto.Mac;
import javax.crypto.spec.SecretKeySpec;
import org.springframework.dao.DataAccessException;
import org.springframework.stereotype.Component;
import org.springframework.web.filter.OncePerRequestFilter;

@Component
public class HmacAuthentication extends OncePerRequestFilter {
    static final String SUBJECT = HmacAuthentication.class.getName() + ".subject";
    static final String ROLE = HmacAuthentication.class.getName() + ".role";
    static final long WINDOW_MS = 120_000;
    private static final int MAX_BODY_BYTES = 16_384;
    private final CommerceProperties props;
    private final NonceStore nonces;
    public HmacAuthentication(CommerceProperties props, NonceStore nonces) { this.props = props; this.nonces = nonces; }

    @Override protected void doFilterInternal(HttpServletRequest request, HttpServletResponse response, FilterChain chain) throws ServletException, IOException {
        HttpServletRequest authenticated;
        try {
            byte[] body = request.getInputStream().readNBytes(MAX_BODY_BYTES + 1);
            if (body.length > MAX_BODY_BYTES) { throw new CommerceException(413, "请求体过大"); }
            String path = request.getRequestURI();
            if (request.getQueryString() != null || path.contains("%") || path.contains(";") || path.contains("\\") || path.contains("//") || path.contains("..")) {
                throw new CommerceException(400, "请求路径非法");
            }
            // 容器已剥离 context 和 DispatcherServlet 前缀；签名仍绑定上面的完整原始 URI。
            String route = request.getPathInfo() == null ? request.getServletPath() : request.getPathInfo();
            String role = Set.of("/internal/payments","/internal/refunds/poll","/internal/refunds/receipt").contains(route) ? "payment" : Set.of("/internal/deliveries/poll","/internal/deliveries/receipt").contains(route) ? "delivery" : "gateway";
            if (route != null && route.startsWith("/internal/") && role.equals("gateway")) { throw new CommerceException(404, "接口不存在"); }
            String subject = single(request, "X-Commerce-Subject");
            if (role.equals("gateway")) { requirePlayer(subject); }
            else if (!(role.equals("delivery")?"trade".equals(subject):props.getPaymentSubject().equals(subject))) { throw new CommerceException(401, "调用方认证失败"); }
            String timestamp = single(request, "X-Commerce-Timestamp");
            String nonce = single(request, "X-Commerce-Nonce");
            String signature = single(request, "X-Commerce-Signature");
            if (!timestamp.matches("[0-9]{1,15}") || !nonce.matches("[A-Za-z0-9_-]{16,128}") || !signature.matches("[0-9a-f]{64}")) {
                throw new CommerceException(401, "调用方认证失败");
            }
            long sent = Long.parseLong(timestamp);
            String secret = role.equals("payment") ? props.getPaymentSecret() : role.equals("delivery")?props.getDeliverySecret():props.getGatewaySecret();
            if(secret==null||secret.getBytes(StandardCharsets.UTF_8).length<32)throw new CommerceException(401,"调用方认证失败");
            byte[] expected = HexFormat.of().parseHex(sign(secret, role, subject, timestamp, nonce, request.getMethod(), path, body));
            if (!MessageDigest.isEqual(expected, HexFormat.of().parseHex(signature))) { throw new CommerceException(401, "调用方认证失败"); }
            nonces.claim(role, nonce, sent);
            request.setAttribute(SUBJECT, subject);
            request.setAttribute(ROLE, role);
            authenticated = new BodyRequest(request, body);
        } catch (CommerceException rejected) {
            response.setStatus(rejected.status()); response.setContentType("application/json;charset=UTF-8");
            response.getWriter().write("{\"error\":\"" + rejected.getMessage() + "\"}"); return;
        } catch (DataAccessException unavailable) {
            response.setStatus(503); response.setContentType("application/json;charset=UTF-8");
            response.getWriter().write("{\"error\":\"认证存储暂不可用\"}"); return;
        }
        chain.doFilter(authenticated, response);
    }

    // v1 + 每行一个字段；主体、时限、nonce、方法、原始路径及正文哈希全部参与签名。
    public static String sign(String secret, String role, String subject, String timestamp, String nonce, String method, String path, byte[] body) {
        try {
            String digest = HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(body));
            String canonical = String.join("\n", "v1", role, subject, timestamp, nonce, method, path, digest);
            Mac mac = Mac.getInstance("HmacSHA256"); mac.init(new SecretKeySpec(secret.getBytes(StandardCharsets.UTF_8), "HmacSHA256"));
            return HexFormat.of().formatHex(mac.doFinal(canonical.getBytes(StandardCharsets.UTF_8)));
        } catch (GeneralSecurityException unavailable) { throw new IllegalStateException("JDK 加密能力不可用"); }
    }
    static void requirePlayer(String subject) {
        try {
            if (subject == null || subject.isEmpty() || subject.length() > 20 || !subject.matches("[1-9][0-9]*") || !Long.toUnsignedString(Long.parseUnsignedLong(subject)).equals(subject)) {
                throw new NumberFormatException();
            }
        } catch (NumberFormatException invalid) { throw new CommerceException(401, "调用方认证失败"); }
    }
    private String single(HttpServletRequest request, String name) {
        List<String> values = Collections.list(request.getHeaders(name));
        if (values.size() != 1 || values.get(0) == null || values.get(0).length() > 256) { throw new CommerceException(401, "调用方认证失败"); }
        return values.get(0);
    }
    private static final class BodyRequest extends HttpServletRequestWrapper {
        private final byte[] body;
        BodyRequest(HttpServletRequest request, byte[] body) { super(request); this.body = body; }
        @Override public ServletInputStream getInputStream() {
            ByteArrayInputStream input = new ByteArrayInputStream(body);
            return new ServletInputStream() {
                @Override public int read() { return input.read(); }
                @Override public boolean isFinished() { return input.available() == 0; }
                @Override public boolean isReady() { return true; }
                @Override public void setReadListener(ReadListener listener) { throw new UnsupportedOperationException("只支持同步 JSON 请求"); }
            };
        }
        @Override public BufferedReader getReader() { return new BufferedReader(new InputStreamReader(getInputStream(), StandardCharsets.UTF_8)); }
    }
}
