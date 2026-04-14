package com.game.gateway.service;

import com.game.gateway.config.CdnProperties;
import com.game.gateway.config.GateProperties;
import com.game.gateway.dto.CdnSignRequest;
import com.game.gateway.dto.CdnSignResponse;
import org.springframework.stereotype.Service;

import javax.crypto.Mac;
import javax.crypto.spec.SecretKeySpec;
import java.nio.charset.StandardCharsets;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;
import java.time.Instant;
import java.util.HexFormat;

@Service
public class CdnSignService {

    private final CdnProperties cdnProps;
    private final GateProperties gateProps;

    public CdnSignService(CdnProperties cdnProps, GateProperties gateProps) {
        this.cdnProps = cdnProps;
        this.gateProps = gateProps;
    }

    public CdnSignResponse sign(CdnSignRequest req) {
        long expireAt = Instant.now().getEpochSecond() + cdnProps.getSignTtlSeconds();
        String data = req.getResourcePath() + "|" + expireAt;

        String sig = hmacSha256Hex(gateProps.getTokenSecret(), data);
        String signedUrl = cdnProps.getBaseUrl() + req.getResourcePath()
                + "?expire=" + expireAt + "&sign=" + sig;

        CdnSignResponse resp = new CdnSignResponse();
        resp.setSignedUrl(signedUrl);
        resp.setExpireAt(expireAt);
        return resp;
    }

    private String hmacSha256Hex(String secret, String data) {
        try {
            Mac mac = Mac.getInstance("HmacSHA256");
            mac.init(new SecretKeySpec(secret.getBytes(StandardCharsets.UTF_8), "HmacSHA256"));
            byte[] raw = mac.doFinal(data.getBytes(StandardCharsets.UTF_8));
            return HexFormat.of().formatHex(raw);
        } catch (NoSuchAlgorithmException | InvalidKeyException e) {
            throw new RuntimeException("HMAC signing failed", e);
        }
    }
}
