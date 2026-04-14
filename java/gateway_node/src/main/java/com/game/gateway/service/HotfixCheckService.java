package com.game.gateway.service;

import com.game.gateway.dto.HotfixCheckRequest;
import com.game.gateway.dto.HotfixCheckResponse;
import org.springframework.stereotype.Service;

@Service
public class HotfixCheckService {

    // TODO: replace with real version comparison from config center or DB
    public HotfixCheckResponse check(HotfixCheckRequest req) {
        HotfixCheckResponse resp = new HotfixCheckResponse();
        resp.setNeedUpdate(false);
        resp.setForceUpdate(false);
        resp.setLatestVersion("1.0.0");
        return resp;
    }
}
