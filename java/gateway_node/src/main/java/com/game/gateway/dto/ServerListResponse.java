package com.game.gateway.dto;

import java.util.List;

public class ServerListResponse {
    private List<ZoneInfoDto> zones;

    public ServerListResponse() {}
    public ServerListResponse(List<ZoneInfoDto> zones) { this.zones = zones; }

    public List<ZoneInfoDto> getZones() { return zones; }
    public void setZones(List<ZoneInfoDto> zones) { this.zones = zones; }
}
