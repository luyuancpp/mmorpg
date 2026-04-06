package types

// ---- /api/server-list ----

type ZoneInfo struct {
	ZoneID      uint32 `json:"zone_id"`
	Name        string `json:"name"`
	Status      string `json:"status"` // "normal", "maintenance", "full"
	Recommended bool   `json:"recommended,omitempty"`
}

type ServerListResponse struct {
	Zones []ZoneInfo `json:"zones"`
}

// ---- /api/assign-gate ----

type AssignGateRequest struct {
	ZoneID uint32 `json:"zone_id"`
}

type AssignGateResponse struct {
	GateIP         string `json:"gate_ip"`
	GatePort       uint32 `json:"gate_port"`
	TokenPayload   []byte `json:"token_payload"`
	TokenSignature []byte `json:"token_signature"`
	TokenDeadline  int64  `json:"token_deadline"`
	Error          string `json:"error,omitempty"`
}

// ---- /api/announcement ----

type AnnouncementItem struct {
	ID        uint32 `json:"id"`
	Title     string `json:"title"`
	Content   string `json:"content"`
	Type      string `json:"type"` // "notice", "maintenance", "update"
	StartTime int64  `json:"start_time"`
	EndTime   int64  `json:"end_time"`
}

type AnnouncementResponse struct {
	Items []AnnouncementItem `json:"items"`
}

// ---- /api/hotfix-check ----

type HotfixCheckRequest struct {
	ClientVersion string `json:"client_version"`
	Platform      string `json:"platform"` // "win", "android", "ios"
}

type HotfixCheckResponse struct {
	NeedUpdate  bool   `json:"need_update"`
	ForceUpdate bool   `json:"force_update"`
	PatchURL    string `json:"patch_url,omitempty"`
	LatestVer   string `json:"latest_version"`
	Changelog   string `json:"changelog,omitempty"`
}

// ---- /api/cdn-sign ----

type CDNSignRequest struct {
	ResourcePath string `json:"resource_path"`
}

type CDNSignResponse struct {
	SignedURL string `json:"signed_url"`
	ExpireAt  int64  `json:"expire_at"`
}
