package com.game.config;

import com.game.config.service.TableService;
import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.context.ConfigurableApplicationContext;

/**
 * Config Node — loads all data tables and exposes query + hot-reload REST endpoints.
 * Suitable for GM tools, operations dashboards, and config validation.
 */
@SpringBootApplication
public class ConfigNodeApplication {

    public static void main(String[] args) {
        ConfigurableApplicationContext ctx = SpringApplication.run(ConfigNodeApplication.class, args);
        TableService tableService = ctx.getBean(TableService.class);
        tableService.loadAll();
        System.out.println("Config Node started — all tables loaded.");
    }
}
