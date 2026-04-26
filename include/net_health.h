#pragma once
#include <Arduino.h>

// ─────────────────────────────────────────────
//  Tuning
// ─────────────────────────────────────────────
static const uint8_t  NH_HISTORY_SIZE       = 20;
static const uint8_t  NH_TARGET_COUNT       = 3;
static const uint32_t NH_CHECK_INTERVAL_MS  = 30000;
static const uint32_t NH_PROBE_TIMEOUT_MS   = 2000;
static const uint8_t  NH_HYSTERESIS_ROUNDS  = 3;

// ─────────────────────────────────────────────
//  Health grade
// ─────────────────────────────────────────────
enum class NetGrade : uint8_t {
    UNKNOWN   = 0,
    EXCELLENT = 1,   // avg < 80ms,  loss == 0%
    GOOD      = 2,   // avg < 200ms, loss <= 10%
    DEGRADED  = 3,   // avg < 500ms, loss <= 40%
    POOR      = 4,   // reachable but bad
    DOWN      = 5    // nothing reachable
};

const char* netGradeLabel(NetGrade g);

// ─────────────────────────────────────────────
//  Per-probe record
// ─────────────────────────────────────────────
struct PingEntry {
    uint32_t latencyMs;
    uint32_t timestamp;
    uint8_t  targetIdx;
    bool     success;
};

// ─────────────────────────────────────────────
//  Stats snapshot
// ─────────────────────────────────────────────
struct NetStats {
    uint32_t avgLatencyMs;
    uint32_t minLatencyMs;
    uint32_t maxLatencyMs;
    uint32_t jitterMs;
    uint8_t  lossPercent;
    NetGrade grade;
    uint32_t lastProbeMs;
};

// ─────────────────────────────────────────────
//  Public state (read-only outside net_health)
// ─────────────────────────────────────────────
extern PingEntry nhHistory[NH_HISTORY_SIZE];
extern uint8_t   nhHistoryCount;
extern uint8_t   nhHistoryHead;
extern NetStats  nhStats;
extern bool      nhActive;

// ─────────────────────────────────────────────
//  Backward compatibility with mood.cpp
// ─────────────────────────────────────────────
extern bool      netHealthIsUp;
extern int       netHealthConsecutiveFails;
extern uint32_t  netHealthLastPing;

// ─────────────────────────────────────────────
//  API
// ─────────────────────────────────────────────
void     netHealthBegin();
void     netHealthEnd();
void     netHealthUpdate();   // call every loop() — NON-BLOCKING
uint32_t netHealthAvgLatency();
void     netHealthPrintStatus();