#include "net_health.h"
#include "wifi_manager.h"
#include <WiFi.h>
#include <WiFiUdp.h>

struct ProbeTarget {
    const char* host;
    const char* label;
};

static const ProbeTarget TARGETS[NH_TARGET_COUNT] = {
    { "1.1.1.1",         "Cloudflare" },
    { "8.8.8.8",         "Google"     },
    { "208.67.222.222",  "OpenDNS"    },
};

static const uint8_t DNS_QUERY[] = {
    0x00, 0x01, 
    0x01, 0x00,  
    0x00, 0x01, 
    0x00, 0x00,  
    0x00, 0x00,  
    0x00, 0x00,  
    0x04, 'p','i','n','g',
    0x04, 't','e','s','t',
    0x00,        
    0x00, 0x01,  
    0x00, 0x01   
};
static const size_t DNS_QUERY_LEN = sizeof(DNS_QUERY);

enum class ProbeState : uint8_t {
    IDLE,
    SENDING,
    WAITING,
    NEXT_TARGET,
    DONE
};

PingEntry nhHistory[NH_HISTORY_SIZE];
uint8_t   nhHistoryCount = 0;
uint8_t   nhHistoryHead  = 0;
NetStats  nhStats        = {};
bool      nhActive       = false;

bool      netHealthIsUp             = true;
int       netHealthConsecutiveFails = 0;
uint32_t  netHealthLastPing         = 0;

static ProbeState  _state          = ProbeState::IDLE;
static uint32_t    _lastRoundMs    = 0;
static uint8_t     _currentTarget  = 0;
static WiFiUDP     _udp;
static uint32_t    _probeStartMs   = 0;
static uint8_t     _betterCount    = 0;
static NetGrade    _pendingGrade   = NetGrade::UNKNOWN;

const char* netGradeLabel(NetGrade g) {
    switch (g) {
        case NetGrade::EXCELLENT: return "EXCELLENT";
        case NetGrade::GOOD:      return "GOOD";
        case NetGrade::DEGRADED:  return "DEGRADED";
        case NetGrade::POOR:      return "POOR";
        case NetGrade::DOWN:      return "DOWN";
        default:                  return "UNKNOWN";
    }
}

static void _pushEntry(uint32_t latencyMs, bool success, uint8_t targetIdx) {
    uint8_t idx;
    if (nhHistoryCount < NH_HISTORY_SIZE) {
        idx = nhHistoryCount;
        nhHistoryCount++;
    } else {
        idx = nhHistoryHead;
        nhHistoryHead = (nhHistoryHead + 1) % NH_HISTORY_SIZE;
    }
    nhHistory[idx].latencyMs  = latencyMs;
    nhHistory[idx].timestamp  = millis();
    nhHistory[idx].targetIdx  = targetIdx;
    nhHistory[idx].success    = success;
}

template<typename Fn>
static void _forEachEntry(Fn fn) {
    if (nhHistoryCount == 0) return;
    uint8_t start = (nhHistoryCount < NH_HISTORY_SIZE) ? 0 : nhHistoryHead;
    for (uint8_t i = 0; i < nhHistoryCount; i++) {
        fn(nhHistory[(start + i) % NH_HISTORY_SIZE]);
    }
}

static void _recalcStats() {
    if (nhHistoryCount == 0) {
        nhStats = {};
        nhStats.grade = NetGrade::UNKNOWN;
        return;
    }

    uint32_t totalMs = 0, minMs = UINT32_MAX, maxMs = 0;
    uint8_t  succCnt = 0, total = 0;

    _forEachEntry([&](const PingEntry& e) {
        total++;
        if (e.success) {
            succCnt++;
            totalMs += e.latencyMs;
            if (e.latencyMs < minMs) minMs = e.latencyMs;
            if (e.latencyMs > maxMs) maxMs = e.latencyMs;
        }
    });

    uint32_t avg = (succCnt > 0) ? (totalMs / succCnt) : 0;

    uint32_t jitter = 0;
    if (succCnt > 1) {
        uint32_t prevLatency = 0;
        uint32_t devSum = 0;
        uint8_t  pairs = 0;
        bool     hasPrev = false;

        _forEachEntry([&](const PingEntry& e) {
            if (e.success) {
                if (hasPrev) {
                    devSum += (e.latencyMs > prevLatency)
                            ? (e.latencyMs - prevLatency)
                            : (prevLatency - e.latencyMs);
                    pairs++;
                }
                prevLatency = e.latencyMs;
                hasPrev = true;
            }
        });

        jitter = (pairs > 0) ? (devSum / pairs) : 0;
    }

    uint8_t lossPct = (total > 0)
        ? (uint8_t)(((uint16_t)(total - succCnt) * 100u) / total)
        : 100;

    nhStats.avgLatencyMs = avg;
    nhStats.minLatencyMs = (minMs == UINT32_MAX) ? 0 : minMs;
    nhStats.maxLatencyMs = maxMs;
    nhStats.jitterMs     = jitter;
    nhStats.lossPercent  = lossPct;
    nhStats.lastProbeMs  = millis();

    NetGrade raw;
    if (succCnt == 0) {
        raw = NetGrade::DOWN;
    } else if (lossPct == 0 && avg < 80) {
        raw = NetGrade::EXCELLENT;
    } else if (lossPct <= 10 && avg < 200) {
        raw = NetGrade::GOOD;
    } else if (lossPct <= 40 && avg < 500) {
        raw = NetGrade::DEGRADED;
    } else {
        raw = NetGrade::POOR;
    }

    uint8_t curOrd = (uint8_t)nhStats.grade;
    uint8_t rawOrd = (uint8_t)raw;

    if (rawOrd >= curOrd) {
        nhStats.grade = raw;
        _betterCount  = 0;
    } else {
        _pendingGrade = raw;
        _betterCount++;
        if (_betterCount >= NH_HYSTERESIS_ROUNDS) {
            nhStats.grade  = _pendingGrade;
            _betterCount   = 0;
        }
    }

    netHealthIsUp = (nhStats.grade != NetGrade::DOWN);
    netHealthLastPing = nhStats.lastProbeMs;

    if (netHealthIsUp) {
        netHealthConsecutiveFails = 0;
    } else {
        int fails = 0;
        for (int i = nhHistoryCount - 1; i >= 0; i--) {
            uint8_t idx = (nhHistoryCount < NH_HISTORY_SIZE)
                          ? i
                          : (nhHistoryHead + i) % NH_HISTORY_SIZE;
            if (!nhHistory[idx].success) fails++;
            else break;
        }
        netHealthConsecutiveFails = fails;
    }
}

static void _sendProbe() {
    _udp.stop();
    _udp.begin(0);
    _udp.beginPacket(TARGETS[_currentTarget].host, 53);
    _udp.write(DNS_QUERY, DNS_QUERY_LEN);
    _udp.endPacket();
    _probeStartMs = millis();
}

void netHealthBegin() {
    if (nhActive) return;
    nhActive        = true;
    nhHistoryCount  = 0;
    nhHistoryHead   = 0;
    nhStats         = {};
    nhStats.grade   = NetGrade::UNKNOWN;
    _state          = ProbeState::IDLE;
    _lastRoundMs    = millis() - NH_CHECK_INTERVAL_MS + 5000;
    _currentTarget  = 0;
    _betterCount    = 0;
    netHealthIsUp   = true;
    netHealthConsecutiveFails = 0;
    Serial.println("[NetHealth] Monitor started (UDP DNS).");
}

void netHealthEnd() {
    if (!nhActive) return;
    _udp.stop();
    _state   = ProbeState::IDLE;
    nhActive = false;
    netHealthIsUp = true;
    netHealthConsecutiveFails = 0;
    Serial.println("[NetHealth] Monitor stopped.");
}

void netHealthUpdate() {
    if (!nhActive) return;

    if (!wifiConnected()) {
        if (_state != ProbeState::IDLE) {
            _udp.stop();
            _state = ProbeState::IDLE;
        }
        return;
    }

    uint32_t now = millis();

    switch (_state) {

    case ProbeState::IDLE:
        if (now - _lastRoundMs >= NH_CHECK_INTERVAL_MS) {
            _lastRoundMs   = now;
            _currentTarget = 0;
            _state         = ProbeState::SENDING;
        }
        break;

    case ProbeState::SENDING:
        _sendProbe();
        _state = ProbeState::WAITING;
        Serial.printf("[NetHealth] Probe → %s ...\n",
                      TARGETS[_currentTarget].label);
        break;

    case ProbeState::WAITING: {
        uint32_t elapsed = now - _probeStartMs;
        bool timedOut = (elapsed >= NH_PROBE_TIMEOUT_MS);

        int packetSize = _udp.parsePacket();

        if (packetSize == 0 && !timedOut) {
            break;  
        }

        bool success = false;
        uint32_t latencyMs = elapsed;

        if (packetSize > 0) {
            uint8_t buf[4];
            int rd = _udp.read(buf, sizeof(buf));
            if (rd >= 4 &&
                buf[0] == 0x00 && buf[1] == 0x01 &&  
                (buf[2] & 0x80)) {                    
                success = true;
            }
        }

        _udp.stop();
        _pushEntry(latencyMs, success, _currentTarget);

        Serial.printf("[NetHealth] %s → %s (%lums)\n",
                      TARGETS[_currentTarget].label,
                      success ? "OK" : "FAIL",
                      (unsigned long)latencyMs);

        _state = ProbeState::NEXT_TARGET;
        break;
    }

    case ProbeState::NEXT_TARGET:
        _currentTarget++;
        if (_currentTarget < NH_TARGET_COUNT) {
            _state = ProbeState::SENDING;
        } else {
            _state = ProbeState::DONE;
        }
        break;

    case ProbeState::DONE:
        _recalcStats();
        _state = ProbeState::IDLE;

        Serial.printf(
            "[NetHealth] Round done — %s | Avg:%lums | "
            "Jitter:%lums | Loss:%u%%\n",
            netGradeLabel(nhStats.grade),
            (unsigned long)nhStats.avgLatencyMs,
            (unsigned long)nhStats.jitterMs,
            (unsigned)nhStats.lossPercent);
        break;
    }
}

uint32_t netHealthAvgLatency() {
    return nhStats.avgLatencyMs;
}

void netHealthPrintStatus() {
    Serial.println();
    Serial.println("[NetHealth] ==========================================");
    Serial.printf("[NetHealth]  Grade:   %s\n",   netGradeLabel(nhStats.grade));
    Serial.printf("[NetHealth]  Avg:     %lums\n", (unsigned long)nhStats.avgLatencyMs);
    Serial.printf("[NetHealth]  Min:     %lums\n", (unsigned long)nhStats.minLatencyMs);
    Serial.printf("[NetHealth]  Max:     %lums\n", (unsigned long)nhStats.maxLatencyMs);
    Serial.printf("[NetHealth]  Jitter:  %lums\n", (unsigned long)nhStats.jitterMs);
    Serial.printf("[NetHealth]  Loss:    %u%%\n",  (unsigned)nhStats.lossPercent);
    Serial.printf("[NetHealth]  Last:    %lus ago\n",
                  nhStats.lastProbeMs > 0
                  ? (unsigned long)((millis() - nhStats.lastProbeMs) / 1000)
                  : 0UL);

    if (nhHistoryCount > 0) {
        Serial.printf("[NetHealth]  History (%u entries):\n", nhHistoryCount);
        uint8_t n = 0;
        _forEachEntry([&](const PingEntry& e) {
            Serial.printf("[NetHealth]    %2u: [%s] %s  %lus ago\n",
                ++n,
                TARGETS[e.targetIdx].label,
                e.success
                    ? (String(e.latencyMs) + "ms").c_str()
                    : "FAIL",
                (unsigned long)((millis() - e.timestamp) / 1000));
        });
    }

    Serial.printf("[NetHealth]  (compat) isUp=%d fails=%d\n",
                  netHealthIsUp, netHealthConsecutiveFails);
    Serial.println("[NetHealth] ==========================================");
    Serial.println();
}