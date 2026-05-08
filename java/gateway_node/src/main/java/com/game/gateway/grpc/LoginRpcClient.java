package com.game.gateway.grpc;

import com.game.gateway.config.LoginGrpcProperties;
import com.game.gateway.dto.LoginResponse;
import com.google.protobuf.CodedInputStream;
import com.google.protobuf.CodedOutputStream;
import io.grpc.CallOptions;
import io.grpc.ManagedChannel;
import io.grpc.ManagedChannelBuilder;
import io.grpc.MethodDescriptor;
import io.grpc.Status;
import io.grpc.StatusRuntimeException;
import io.grpc.stub.ClientCalls;
import jakarta.annotation.PostConstruct;
import jakarta.annotation.PreDestroy;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.boot.context.properties.EnableConfigurationProperties;
import org.springframework.stereotype.Component;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Thin gRPC client for {@code go/login}'s {@code ClientPlayerLogin.Login} RPC.
 *
 * <p><b>Design choice — no protoc-generated Java classes.</b> The gateway
 * already has hundreds of proto files in {@code proto/}; pulling them all
 * into the Java build (or carefully selecting just login.proto and its
 * imports) creates a heavy dependency on the C++/Go-centric proto pipeline.
 *
 * <p>Instead we hand-code the wire format for two messages
 * ({@code LoginRequest} and {@code LoginResponse}) using the protobuf-java
 * runtime that's already on the classpath. Field tags and types come straight
 * from {@code proto/login/login.proto}. As long as the proto fields stay
 * additive (which is the wire-format contract), this client keeps working
 * across upgrades. Any breaking field change shows up at runtime in
 * {@code parseResponse} — same blast radius as a generated stub anyway.
 *
 * <p>Channel lifecycle: one persistent {@link ManagedChannel} per endpoint,
 * created at startup and reused for every call (HTTP/2 multiplexing). This
 * is the same long-lived-connection contract the C++ {@code GrpcChannelCache}
 * holds for cpp gate→login.
 */
@Component
@EnableConfigurationProperties(LoginGrpcProperties.class)
public class LoginRpcClient {

    private static final Logger log = LoggerFactory.getLogger(LoginRpcClient.class);

    /** Fully-qualified name from proto/login/login.proto. */
    private static final String SERVICE = "ClientPlayerLogin";
    private static final String METHOD  = SERVICE + "/Login";

    private final LoginGrpcProperties props;
    private final List<ManagedChannel> channels = new ArrayList<>();
    private final AtomicInteger rr = new AtomicInteger();

    private final MethodDescriptor<LoginRequestProto, LoginResponseProto> methodDescriptor;

    public LoginRpcClient(LoginGrpcProperties props) {
        this.props = props;
        this.methodDescriptor = MethodDescriptor.<LoginRequestProto, LoginResponseProto>newBuilder()
                .setType(MethodDescriptor.MethodType.UNARY)
                .setFullMethodName(METHOD)
                .setRequestMarshaller(new RequestMarshaller())
                .setResponseMarshaller(new ResponseMarshaller())
                .build();
    }

    @PostConstruct
    void start() {
        String[] eps = props.getEndpoints().split(",");
        for (String raw : eps) {
            String ep = raw.trim();
            if (ep.isEmpty()) continue;
            String[] hp = ep.split(":");
            if (hp.length != 2) {
                log.warn("login.grpc endpoint malformed, skipped: {}", ep);
                continue;
            }
            ManagedChannel ch = ManagedChannelBuilder.forAddress(hp[0], Integer.parseInt(hp[1]))
                    .usePlaintext()
                    .keepAliveTime(30, TimeUnit.SECONDS)
                    .keepAliveTimeout(10, TimeUnit.SECONDS)
                    .keepAliveWithoutCalls(true)
                    .build();
            channels.add(ch);
            log.info("login.rpc channel up: {}", ep);
        }
        if (channels.isEmpty()) {
            log.warn("LoginRpcClient has no endpoints; /api/login will fail");
        }
    }

    @PreDestroy
    void stop() {
        for (var ch : channels) {
            ch.shutdown();
            try {
                ch.awaitTermination(5, TimeUnit.SECONDS);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }
    }

    /** Calls {@code ClientPlayerLogin.Login}. Throws on terminal failure. */
    public LoginResponseProto login(LoginRequestProto req) {
        if (channels.isEmpty()) {
            throw new IllegalStateException("login.rpc no endpoint");
        }
        ManagedChannel ch = channels.get(Math.floorMod(rr.getAndIncrement(), channels.size()));
        CallOptions opts = CallOptions.DEFAULT.withDeadlineAfter(props.getTimeoutMs(), TimeUnit.MILLISECONDS);

        int attempts = 1 + Math.max(0, props.getRetry());
        StatusRuntimeException last = null;
        for (int i = 0; i < attempts; i++) {
            try {
                return ClientCalls.blockingUnaryCall(ch, methodDescriptor, opts, req);
            } catch (StatusRuntimeException e) {
                last = e;
                Status.Code c = e.getStatus().getCode();
                if (c != Status.Code.UNAVAILABLE && c != Status.Code.DEADLINE_EXCEEDED) {
                    throw e;        // non-retriable: don't waste budget
                }
                log.warn("login.rpc retriable error attempt {}/{}: {}", i + 1, attempts, c);
            }
        }
        throw last;
    }

    // ──────────────────── Protobuf wire encoding ─────────────────────

    /**
     * proto/login/login.proto LoginRequest:
     *   string account = 1;
     *   string password = 2;
     *   string auth_type = 3;
     *   string auth_token = 4;
     *
     * (Gateway also forwards device_id/zone_id via metadata; the proto today
     * does not have those fields, so we don't write them — additive proto
     * upgrade later can pick them up without changing this file's wire output.)
     */
    public static final class LoginRequestProto {
        final String account;
        final String password;
        final String authType;
        final String authToken;

        public LoginRequestProto(String account, String password, String authType, String authToken) {
            this.account   = nullToEmpty(account);
            this.password  = nullToEmpty(password);
            this.authType  = nullToEmpty(authType);
            this.authToken = nullToEmpty(authToken);
        }

        byte[] toBytes() throws IOException {
            ByteArrayOutputStream baos = new ByteArrayOutputStream(64);
            CodedOutputStream out = CodedOutputStream.newInstance(baos);
            if (!account.isEmpty())   out.writeString(1, account);
            if (!password.isEmpty())  out.writeString(2, password);
            if (!authType.isEmpty())  out.writeString(3, authType);
            if (!authToken.isEmpty()) out.writeString(4, authToken);
            out.flush();
            return baos.toByteArray();
        }
    }

    /**
     * proto/login/login.proto LoginResponse:
     *   TipInfoMessage error_message = 1;                    (group/message — skipped, parsed lazily)
     *   repeated AccountSimplePlayerWrapper players = 2;     (parsed minimally — we only forward player ids/names if needed later)
     *   string access_token = 3;
     *   string refresh_token = 4;
     *   int64  access_token_expire = 5;
     *   int64  refresh_token_expire = 6;
     */
    public static final class LoginResponseProto {
        public Integer errorCode;       // populated if error_message present and we can crack it
        public String  errorMessage;
        public List<LoginResponse.PlayerInfo> players = new ArrayList<>();
        public String  accessToken;
        public String  refreshToken;
        public long    accessTokenExpire;
        public long    refreshTokenExpire;

        public boolean hasError() {
            return (errorCode != null && errorCode != 0)
                    || (errorMessage != null && !errorMessage.isEmpty() && (errorCode == null || errorCode != 0));
        }
    }

    private static String nullToEmpty(String s) { return s == null ? "" : s; }

    private static final class RequestMarshaller implements MethodDescriptor.Marshaller<LoginRequestProto> {
        @Override
        public InputStream stream(LoginRequestProto value) {
            try {
                return new java.io.ByteArrayInputStream(value.toBytes());
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        public LoginRequestProto parse(InputStream stream) {
            // gateway never receives a request — server-side parsing happens in go-zero
            throw new UnsupportedOperationException();
        }
    }

    private static final class ResponseMarshaller implements MethodDescriptor.Marshaller<LoginResponseProto> {
        @Override
        public InputStream stream(LoginResponseProto value) {
            throw new UnsupportedOperationException();
        }

        @Override
        public LoginResponseProto parse(InputStream stream) {
            try {
                byte[] bytes = stream.readAllBytes();
                return parseResponse(bytes);
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
        }
    }

    /**
     * Hand-rolled parser that only reads fields we care about. Unknown fields
     * are skipped using {@link CodedInputStream#skipField}, so additive proto
     * upgrades remain wire-compatible.
     */
    private static LoginResponseProto parseResponse(byte[] bytes) throws IOException {
        LoginResponseProto resp = new LoginResponseProto();
        CodedInputStream in = CodedInputStream.newInstance(bytes);
        while (!in.isAtEnd()) {
            int tag = in.readTag();
            int field = tag >>> 3;
            switch (field) {
                case 1 -> {
                    // error_message: nested message — read length-prefixed and try to extract
                    // a numeric "id" field (TipInfoMessage commonly carries an int id).
                    int len = in.readRawVarint32();
                    int oldLimit = in.pushLimit(len);
                    while (!in.isAtEnd()) {
                        int subTag = in.readTag();
                        int subField = subTag >>> 3;
                        int wire = subTag & 0x7;
                        if (subField == 1 && wire == 0) {
                            resp.errorCode = (int) in.readInt64();
                        } else if (subField == 2 && wire == 2) {
                            resp.errorMessage = in.readString();
                        } else {
                            in.skipField(subTag);
                        }
                    }
                    in.popLimit(oldLimit);
                }
                case 2 -> {
                    // players: skip detailed parse for now — controller doesn't surface them yet.
                    in.skipField(tag);
                }
                case 3 -> resp.accessToken = in.readString();
                case 4 -> resp.refreshToken = in.readString();
                case 5 -> resp.accessTokenExpire = in.readInt64();
                case 6 -> resp.refreshTokenExpire = in.readInt64();
                default -> in.skipField(tag);
            }
        }
        return resp;
    }
}
