package com.game.gateway.entity;

import jakarta.persistence.*;

@Entity
@Table(name = "zone_whitelist",
        uniqueConstraints = @UniqueConstraint(columnNames = {"zone_id", "account_id"}))
public class ZoneWhitelist {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column(name = "zone_id", nullable = false)
    private Long zoneId;

    @Column(name = "account_id", nullable = false)
    private Long accountId;

    @Column(name = "note", length = 128)
    private String note = "";

    public Long getId() { return id; }
    public void setId(Long id) { this.id = id; }

    public Long getZoneId() { return zoneId; }
    public void setZoneId(Long zoneId) { this.zoneId = zoneId; }

    public Long getAccountId() { return accountId; }
    public void setAccountId(Long accountId) { this.accountId = accountId; }

    public String getNote() { return note; }
    public void setNote(String note) { this.note = note; }
}
