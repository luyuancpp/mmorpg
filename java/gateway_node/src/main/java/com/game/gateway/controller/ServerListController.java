package com.game.gateway.controller;

import com.game.gateway.dto.ServerListResponse;
import com.game.gateway.service.ServerListService;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

@RestController
@RequestMapping("/api")
public class ServerListController {

    private final ServerListService serverListService;

    public ServerListController(ServerListService serverListService) {
        this.serverListService = serverListService;
    }

    @GetMapping("/server-list")
    public ServerListResponse getServerList() {
        return serverListService.getServerList();
    }
}
