package com.game.mall;

import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.boot.context.properties.EnableConfigurationProperties;

@SpringBootApplication
@EnableConfigurationProperties(CommerceProperties.class)
public class MallGameApplication {
    public static void main(String[] args) { SpringApplication.run(MallGameApplication.class, args); }
}
