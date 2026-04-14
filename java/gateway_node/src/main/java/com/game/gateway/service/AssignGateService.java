package com.game.gateway.service;

import com.game.gateway.config.GateProperties;
import com.game.gateway.dto.AssignGateRequest;
import com.game.gateway.dto.AssignGateResponse;
import com.game.gateway.etcd.GateWatcher;
import com.game.gateway.etcd.NodeInfoRecord;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;

import javax.crypto.Mac;
import javax.crypto.spec.SecretKeySpec;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;
import java.time.Instant;
import java.util.Comparator;
import java.util.HexFormat;
import java.util.List;

@Service
public class AssignGateService {

    private static final Logger log = LoggerFactory.getLogger(AssignGateService.class);

    private final GateWatcher gateWatcher;
    private final GateProperties gateProps;

    public AssignGateService(GateWatcher gateWatcher, GateProperties gateProps) {
        this.gateWatcher = gateWatcher;
        this.gateProps = gateProps;
    }

    public AssignGateResponse assignGate(AssignGateRequest req) {
        List<NodeInfoRecord> allNodes = gateWatcher.fetchAllGateNodes();

        // Filter by zone, require valid endpoint
        List<NodeInfoRecord> candidates = allNodes.stream()
                .filter(n -> n.getEndpoint() != null)
                .filter(n -> req.getZoneId() == 0 || n.getZoneId() == req.getZoneId())
                .sorted(Comparator.comparingLong(NodeInfoRecord::getPlayerCount))
                .toList();

        if (candidates.isEmpty()) {
            AssignGateResponse resp = new AssignGateResponse();
            resp.setError("no gate available for requested zone");
            return resp;
        }

        NodeInfoRecord best = candidates.getFirst();
        long expireTs = Instant.now().getEpochSecond() + gateProps.getTokenTtlSeconds();

        // Build payload: simple binary format matching Go's proto.Marshal of GateTokenPayload
        // Field 1 (gate_node_id): varint, Field 2 (zone_id): varint, Field 3 (expire_timestamp): varint
        byte[] payload = buildGateTokenPayload(best.getNodeId(), best.getZoneId(), expireTs);
        byte[] signature = signHmac(gateProps.getTokenSecret(), payload);

        AssignGateResponse resp = new AssignGateResponse();
        resp.setGateIp(best.getEndpoint().getIp());
        resp.setGatePort(best.getEndpoint().getPort());
        resp.setTokenPayload(payload);
        resp.setTokenSignature(signature);
        resp.setTokenDeadline(expireTs);
        return resp;
    }

    /**
     * Encodes GateTokenPayload as protobuf wire format:
     *   field 1 (uint32 gate_node_id) = varint
     *   field 2 (uint32 zone_id) = varint
     *   field 3 (int64 expire_timestamp) = varint
     */
    private byte[] buildGateTokenPayload(long gateNodeId, long zoneId, long expireTimestamp) {
        ByteBuffer buf = ByteBuffer.allocate(30).order(ByteOrder.LITTLE_ENDIAN);
        // field 1, wire type 0 (varint): tag = (1 << 3) | 0 = 0x08
        writeTagAndVarint(buf, 1, gateNodeId);
        // field 2, wire type 0: tag = (2 << 3) | 0 = 0x10
        writeTagAndVarint(buf, 2, zoneId);
        // field 3, wire type 0: tag = (3 << 3) | 0 = 0x18
        writeTagAndVarint(buf, 3, expireTimestamp);

        byte[] result = new byte[buf.position()];
        buf.flip();
        buf.get(result);
        return result;
    }

    private void writeTagAndVarint(ByteBuffer buf, int fieldNumber, long value) {
        writeVarint(buf, (fieldNumber << 3)); // wire type 0
        writeVarint(buf, value);
    }

    private void writeVarint(ByteBuffer buf, long value) {
        while ((value & ~0x7FL) != 0) {
            buf.put((byte) ((value & 0x7F) | 0x80));
            value >>>= 7;
        }
        buf.put((byte) (value & 0x7F));
    }

    /**
     * HMAC-SHA256, returns hex-encoded string as bytes (matches Go implementation).
     */
    private byte[] signHmac(String secret, byte[] data) {
        try {
            Mac mac = Mac.getInstance("HmacSHA256");
            mac.init(new SecretKeySpec(secret.getBytes(StandardCharsets.UTF_8), "HmacSHA256"));
            byte[] raw = mac.doFinal(data);
            return HexFormat.of().formatHex(raw).getBytes(StandardCharsets.UTF_8);
        } catch (NoSuchAlgorithmException | InvalidKeyException e) {
            throw new RuntimeException("HMAC signing failed", e);
        }
    }
}
