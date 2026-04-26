#pragma once
#include <Arduino.h>

static const uint8_t  NH_HISTORY_SIZE       = 20;
static const uint8_t  NH_TARGET_COUNT       = 3;
static const uint32_t NH_CHECK_INTERVAL_MS  = 30000;
static const uint32_t NH_PROBE_TIMEOUT_MS   = 2000;
static const uint8_t  NH_HYSTERESIS_ROUNDS  = 3;

enum class NetGrade : uint8_t {
    UNKNOWN   = 0,
    EXCELLENT = 1,   
    GOOD      = 2,   
    DEGRADED  = 3,   
    POOR      = 4,   
    DOWN      = 5    
};

const char* netGradeLabel(NetGrade g);

struct PingEntry {
    uint32_t latencyMs;
    uint32_t timestamp;
    uint8_t  targetIdx;
    bool     success;
};

struct NetStats {
    uint32_t avgLatencyMs;
    uint32_t minLatencyMs;
    uint32_t maxLatencyMs;
    uint32_t jitterMs;
    uint8_t  lossPercent;
    NetGrade grade;
    uint32_t lastProbeMs;
};

extern PingEntry nhHistory[NH_HISTORY_SIZE];
extern uint8_t   nhHistoryCount;
extern uint8_t   nhHistoryHead;
extern NetStats  nhStats;
extern bool      nhActive;

extern bool      netHealthIsUp;
extern int       netHealthConsecutiveFails;
extern uint32_t  netHealthLastPing;

void     netHealthBegin();
void     netHealthEnd();
void     netHealthUpdate();  
uint32_t netHealthAvgLatency();
void     netHealthPrintStatus();