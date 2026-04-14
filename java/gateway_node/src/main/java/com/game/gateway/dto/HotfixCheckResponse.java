package com.game.gateway.dto;

import com.fasterxml.jackson.annotation.JsonInclude;

@JsonInclude(JsonInclude.Include.NON_NULL)
public class HotfixCheckResponse {
    private boolean needUpdate;
    private boolean forceUpdate;
    private String patchUrl;
    private String latestVersion;
    private String changelog;

    public boolean isNeedUpdate() { return needUpdate; }
    public void setNeedUpdate(boolean needUpdate) { this.needUpdate = needUpdate; }

    public boolean isForceUpdate() { return forceUpdate; }
    public void setForceUpdate(boolean forceUpdate) { this.forceUpdate = forceUpdate; }

    public String getPatchUrl() { return patchUrl; }
    public void setPatchUrl(String patchUrl) { this.patchUrl = patchUrl; }

    public String getLatestVersion() { return latestVersion; }
    public void setLatestVersion(String latestVersion) { this.latestVersion = latestVersion; }

    public String getChangelog() { return changelog; }
    public void setChangelog(String changelog) { this.changelog = changelog; }
}
