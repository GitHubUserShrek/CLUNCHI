#ifndef PORTAL_H
#define PORTAL_H

#include <Arduino.h>

const char PORTAL_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>CLUNCHI SETUP</title>
    <meta name="viewport" content="width=device-width,initial-scale=1">
    <style>
        *{margin:0;padding:0;box-sizing:border-box}
        body{font-family:'Courier New',monospace;background:#000;color:#0f0;text-align:center;padding:30px 15px;min-height:100vh}
        .box{max-width:360px;margin:0 auto;border:2px solid #0f0;padding:20px;background:#000}
        h2{font-size:26px;letter-spacing:6px;margin-bottom:4px;text-shadow:0 0 10px #0f0}
        .subtitle{font-size:10px;color:#080;letter-spacing:3px;margin-bottom:20px}
        .status{background:#111;border:1px dashed #0f0;padding:14px;margin:20px 0;font-size:12px;color:#0f0;text-align:left}
        input[type=text],input[type=password]{width:100%;padding:14px;margin:12px 0;background:#000;border:1px solid #0f0;color:#0f0;font-family:monospace;font-size:16px;outline:none}
        button{width:100%;padding:16px;margin:10px 0;background:#000;color:#0f0;border:2px solid #0f0;font:bold 16px monospace;cursor:pointer;text-transform:uppercase;letter-spacing:2px}
        button:hover{background:#0f0;color:#000}
        button:active{background:#080}
        #clearBtn{background:transparent;color:#f00;border:1px solid #f00;font-size:13px;padding:12px;margin-top:20px;display:none}
        .footer{font-size:9px;color:#040;margin-top:30px;letter-spacing:1px;opacity:0.7}
        #loader{position:fixed;top:0;left:0;width:100%;height:100%;background:#000;color:#0f0;font-size:22px;font-weight:bold;display:none;flex-direction:column;justify-content:center;align-items:center;z-index:999}
        .blink{animation:blink 1s steps(1) infinite}
        @keyframes blink{0%,100%{opacity:1}50%{opacity:0}}
    </style>
</head>
<body>

<div id="loader">
    APPLYING CONFIG<span class="blink">...</span><br><br>
    DEVICE REBOOTING
</div>

<div class="box">
    <h2>CLUNCHI</h2>
    <div class="subtitle">2.4GHZ WIFI SETUP PORTAL</div>

    <div id="statusBox" class="status">SCANNING STORAGE...</div>

    <input type="text" id="ssid" placeholder="WIFI NAME" autocomplete="off" spellcheck="false">
    <input type="password" id="pass" placeholder="PASSWORD (LEAVE BLANK IF OPEN)" spellcheck="false">

    <button onclick="save()">CONNECT TO NETWORK</button>
    <button id="clearBtn" onclick="if(confirm('ERASE SAVED WIFI?'))clearWiFi()">ERASE SAVED WIFI</button>

    <div class="footer">
        CLUNCHI NETWORK ANALYZER BETA v1.0
    </div>
</div>

<script>
    // check if we already have saved wifi
    setTimeout(() => {
        const xhr = new XMLHttpRequest();
        xhr.open('GET', '/status');
        xhr.onload = function() {
            if (xhr.status === 200) {
                const data = JSON.parse(xhr.responseText);
                const box = document.getElementById('statusBox');
                const btn = document.getElementById('clearBtn');
                if (data.saved && data.ssid) {
                    box.innerHTML = '<b>SAVED NETWORK:</b><br>' + data.ssid;
                    btn.style.display = 'block';
                } else {
                    box.innerHTML = 'NO WIFI SAVED';
                }
            } else {
                document.getElementById('statusBox').innerHTML = 'STATUS CHECK FAILED';
            }
        };
        xhr.onerror = () => { document.getElementById('statusBox').innerHTML = 'OFFLINE'; };
        xhr.send();
    }, 800);

    function save() {
        const s = document.getElementById('ssid').value.trim();
        const p = document.getElementById('pass').value;
        if (!s) { alert('GIVE ME AN SSID'); return; }

        document.getElementById('loader').style.display = 'flex';

        // FIX: Send JSON instead of form data
        const payload = {
            ssid: s,
            password: p
        };

        const xhr = new XMLHttpRequest();
        xhr.open('POST', '/save');
        xhr.setRequestHeader('Content-Type', 'application/json');
        xhr.send(JSON.stringify(payload));
    }

    function clearWiFi() {
        document.getElementById('loader').style.display = 'flex';
        const xhr = new XMLHttpRequest();
        xhr.open('POST', '/clear');
        xhr.send();
    }
</script>
</body>
</html>
)rawliteral";

#endif