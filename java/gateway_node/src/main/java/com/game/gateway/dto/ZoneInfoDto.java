package com.game.gateway.dto;

import com.fasterxml.jackson.annotation.JsonInclude;

@JsonInclude(JsonInclude.Include.NON_NULL)
public class ZoneInfoDto {

    private long zoneId;
    private String name;
    private ZoneDisplayStatus status;
    private LoadLevel loadLevel;
    private String maintenanceMsg;
    private Long openTime;       // epoch seconds, null if not PREVIEW
    private boolean isNew;
    private boolean recommended;

    public long getZoneId() { return zoneId; }
    public void setZoneId(long zoneId) { this.zoneId = zoneId; }

    public String getName() { return name; }
    public void setName(String name) { this.name = name; }

    public ZoneDisplayStatus getStatus() { return status; }
    public void setStatus(ZoneDisplayStatus status) { this.status = status; }

    public LoadLevel getLoadLevel() { return loadLevel; }
    public void setLoadLevel(LoadLevel loadLevel) { this.loadLevel = loadLevel; }

    public String getMaintenanceMsg() { return maintenanceMsg; }
    public void setMaintenanceMsg(String maintenanceMsg) { this.maintenanceMsg = maintenanceMsg; }

    public Long getOpenTime() { return openTime; }
    public void setOpenTime(Long openTime) { this.openTime = openTime; }

    public boolean isNew() { return isNew; }
    public void setNew(boolean isNew) { this.isNew = isNew; }

    public boolean isRecommended() { return recommended; }
    public void setRecommended(boolean recommended) { this.recommended = recommended; }
}
