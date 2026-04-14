package com.game.gateway.etcd;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.game.gateway.config.GateProperties;
import io.etcd.jetcd.ByteSequence;
import io.etcd.jetcd.Client;
import io.etcd.jetcd.KeyValue;
import io.etcd.jetcd.kv.GetResponse;
import io.etcd.jetcd.options.GetOption;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Component;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.TimeUnit;

@Component
public class GateWatcher {

    private static final Logger log = LoggerFactory.getLogger(GateWatcher.class);
    private static final ObjectMapper MAPPER = new ObjectMapper();

    private final Client etcdClient;
    private final GateProperties gateProps;

    public GateWatcher(Client etcdClient, GateProperties gateProps) {
        this.etcdClient = etcdClient;
        this.gateProps = gateProps;
    }

    /**
     * Fetches all gate nodes currently registered in etcd.
     */
    public List<NodeInfoRecord> fetchAllGateNodes() {
        return fetchNodesByPrefix(NodeType.GATE_PREFIX);
    }

    /**
     * Fetches all scene nodes currently registered in etcd.
     */
    public List<NodeInfoRecord> fetchAllSceneNodes() {
        return fetchNodesByPrefix(NodeType.SCENE_PREFIX);
    }

    private List<NodeInfoRecord> fetchNodesByPrefix(String prefix) {
        try {
            ByteSequence prefixKey = ByteSequence.from(prefix, StandardCharsets.UTF_8);
            GetOption option = GetOption.builder().isPrefix(true).build();
            GetResponse resp = etcdClient.getKVClient()
                    .get(prefixKey, option)
                    .get(gateProps.getDiscoveryTimeoutMs(), TimeUnit.MILLISECONDS);

            List<NodeInfoRecord> nodes = new ArrayList<>();
            for (KeyValue kv : resp.getKvs()) {
                try {
                    String json = kv.getValue().toString(StandardCharsets.UTF_8);
                    NodeInfoRecord info = MAPPER.readValue(json, NodeInfoRecord.class);
                    nodes.add(info);
                } catch (Exception e) {
                    log.warn("Failed to parse NodeInfo from key={}: {}",
                            kv.getKey().toString(StandardCharsets.UTF_8), e.getMessage());
                }
            }
            return nodes;
        } catch (Exception e) {
            log.error("Failed to fetch nodes with prefix {}: {}", prefix, e.getMessage());
            return Collections.emptyList();
        }
    }
}
