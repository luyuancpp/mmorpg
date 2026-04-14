package com.game.gateway.config;

import io.etcd.jetcd.Client;
import jakarta.annotation.PreDestroy;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

@Configuration
public class EtcdClientConfig {

    private Client client;

    @Bean
    public Client etcdClient(EtcdProperties props) {
        client = Client.builder()
                .endpoints(props.getEndpoints().toArray(String[]::new))
                .build();
        return client;
    }

    @PreDestroy
    public void close() {
        if (client != null) {
            client.close();
        }
    }
}
