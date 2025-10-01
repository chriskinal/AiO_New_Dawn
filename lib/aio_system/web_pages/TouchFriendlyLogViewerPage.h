// TouchFriendlyLogViewerPage.h
// Touch-optimized log viewer page with filtering

#ifndef TOUCH_FRIENDLY_LOG_VIEWER_PAGE_H
#define TOUCH_FRIENDLY_LOG_VIEWER_PAGE_H

#include <Arduino.h>

const char TOUCH_FRIENDLY_LOG_VIEWER_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <meta name="apple-mobile-web-app-capable" content="yes">
    <title>Log Viewer - AiO New Dawn</title>
    <link rel="stylesheet" href="/touch.css">
    <style>
        /* Log viewer specific styles */
        .filter-bar {
            background: #ecf0f1;
            padding: 15px;
            border-radius: 8px;
            margin-bottom: 20px;
        }

        .filter-row {
            display: flex;
            gap: 10px;
            margin-bottom: 10px;
            flex-wrap: wrap;
        }

        .filter-btn {
            flex: 1;
            min-width: 100px;
            padding: 12px;
            border: 2px solid #bdc3c7;
            border-radius: 8px;
            background: white;
            font-size: 16px;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.3s;
        }

        .filter-btn.active {
            background: #3498db;
            color: white;
            border-color: #2980b9;
        }

        .log-container {
            background: #2c3e50;
            color: #ecf0f1;
            padding: 15px;
            border-radius: 8px;
            font-family: 'Monaco', 'Courier New', monospace;
            font-size: 14px;
            max-height: 500px;
            overflow-y: auto;
            margin-bottom: 20px;
        }

        .log-entry {
            padding: 8px 0;
            border-bottom: 1px solid #34495e;
        }

        .log-entry:last-child {
            border-bottom: none;
        }

        .log-time {
            color: #95a5a6;
            font-weight: 600;
        }

        .log-severity {
            font-weight: 700;
            padding: 2px 8px;
            border-radius: 4px;
            margin: 0 5px;
        }

        .severity-0, .severity-1, .severity-2 { color: #e74c3c; } /* EMERG, ALERT, CRIT */
        .severity-3 { color: #e67e22; } /* ERROR */
        .severity-4 { color: #f39c12; } /* WARN */
        .severity-5 { color: #3498db; } /* NOTICE */
        .severity-6 { color: #2ecc71; } /* INFO */
        .severity-7 { color: #9b59b6; } /* DEBUG */

        .log-source {
            color: #1abc9c;
            font-weight: 600;
        }

        .log-message {
            color: #ecf0f1;
            word-wrap: break-word;
        }

        .controls {
            display: flex;
            gap: 15px;
            margin-bottom: 20px;
        }

        .control-btn {
            flex: 1;
            padding: 15px;
            font-size: 18px;
            border-radius: 8px;
            border: none;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.3s;
        }

        .refresh-btn {
            background: #2ecc71;
            color: white;
        }

        .refresh-btn:active {
            background: #27ae60;
        }

        .auto-refresh-btn {
            background: #3498db;
            color: white;
        }

        .auto-refresh-btn.active {
            background: #e74c3c;
        }

        .clear-btn {
            background: #95a5a6;
            color: white;
        }

        .clear-btn:active {
            background: #7f8c8d;
        }

        .stats {
            text-align: center;
            padding: 10px;
            background: #ecf0f1;
            border-radius: 8px;
            margin-bottom: 20px;
            font-size: 16px;
            color: #2c3e50;
        }

        @media (max-width: 600px) {
            .filter-btn {
                min-width: 80px;
                font-size: 14px;
                padding: 10px;
            }

            .log-container {
                font-size: 12px;
            }
        }
    </style>
    <script>
        let logs = [];
        let autoRefresh = false;
        let refreshInterval = null;

        // Filters
        let severityFilter = -1;  // -1 = all
        let sourceFilter = -1;    // -1 = all

        const severityNames = ['EMERG', 'ALERT', 'CRIT', 'ERROR', 'WARN', 'NOTICE', 'INFO', 'DEBUG'];
        const sourceNames = ['SYS', 'NET', 'GNSS', 'IMU', 'STEER', 'MACH', 'CAN', 'CFG', 'USER'];

        function formatTime(ms) {
            const seconds = Math.floor(ms / 1000);
            const hours = Math.floor(seconds / 3600) % 24;
            const minutes = Math.floor(seconds / 60) % 60;
            const secs = seconds % 60;
            const millis = ms % 1000;
            return `${hours.toString().padStart(2, '0')}:${minutes.toString().padStart(2, '0')}:${secs.toString().padStart(2, '0')}.${millis.toString().padStart(3, '0')}`;
        }

        function loadLogs() {
            fetch('/api/logs/data')
            .then(response => response.json())
            .then(data => {
                logs = data.logs;
                renderLogs();
            })
            .catch(error => {
                console.error('Error loading logs:', error);
            });
        }

        function renderLogs() {
            const container = document.getElementById('logContainer');

            // Filter logs
            let filtered = logs.filter(log => {
                if (severityFilter >= 0 && log.severity !== severityFilter) return false;
                if (sourceFilter >= 0 && log.source !== sourceFilter) return false;
                return true;
            });

            // Update stats
            document.getElementById('stats').innerHTML =
                `Showing ${filtered.length} of ${logs.length} logs`;

            // Render filtered logs
            if (filtered.length === 0) {
                container.innerHTML = '<div class="log-entry">No logs to display</div>';
                return;
            }

            container.innerHTML = filtered.map(log => {
                return `<div class="log-entry">
                    <span class="log-time">[${formatTime(log.timestamp)}]</span>
                    <span class="log-severity severity-${log.severity}">${log.severityName}</span>
                    <span class="log-source">${log.sourceName}</span>
                    <span class="log-message">: ${log.message}</span>
                </div>`;
            }).join('');

            // Auto-scroll to bottom
            container.scrollTop = container.scrollHeight;
        }

        function setSeverityFilter(level) {
            severityFilter = (severityFilter === level) ? -1 : level;

            // Update button states
            document.querySelectorAll('.severity-filter-btn').forEach(btn => {
                btn.classList.remove('active');
            });

            if (severityFilter >= 0) {
                const btn = document.querySelector(`[data-severity="${severityFilter}"]`);
                if (btn) btn.classList.add('active');
            }

            renderLogs();
        }

        function setSourceFilter(source) {
            sourceFilter = (sourceFilter === source) ? -1 : source;

            // Update button states
            document.querySelectorAll('.source-filter-btn').forEach(btn => {
                btn.classList.remove('active');
            });

            if (sourceFilter >= 0) {
                const btn = document.querySelector(`[data-source="${sourceFilter}"]`);
                if (btn) btn.classList.add('active');
            }

            renderLogs();
        }

        function toggleAutoRefresh() {
            autoRefresh = !autoRefresh;
            const btn = document.getElementById('autoRefreshBtn');

            if (autoRefresh) {
                btn.classList.add('active');
                btn.textContent = 'Auto: ON';
                refreshInterval = setInterval(loadLogs, 2000);  // Refresh every 2 seconds
            } else {
                btn.classList.remove('active');
                btn.textContent = 'Auto: OFF';
                if (refreshInterval) {
                    clearInterval(refreshInterval);
                    refreshInterval = null;
                }
            }
        }

        function clearFilters() {
            severityFilter = -1;
            sourceFilter = -1;

            document.querySelectorAll('.filter-btn').forEach(btn => {
                btn.classList.remove('active');
            });

            renderLogs();
        }

        // Load logs on page load
        window.addEventListener('DOMContentLoaded', function() {
            loadLogs();
        });
    </script>
</head>
<body>
    <div class="header">
        <h1>📋 Log Viewer</h1>
    </div>

    <div class="container">
        <div class="nav-button">
            <button class="big-button" onclick="location.href='/'">← Home</button>
        </div>

        <div class="stats" id="stats">Loading...</div>

        <div class="controls">
            <button class="control-btn refresh-btn" onclick="loadLogs()">🔄 Refresh</button>
            <button class="control-btn auto-refresh-btn" id="autoRefreshBtn" onclick="toggleAutoRefresh()">Auto: OFF</button>
            <button class="control-btn clear-btn" onclick="clearFilters()">✖ Clear Filters</button>
        </div>

        <div class="filter-bar">
            <h3 style="margin-top: 0;">Filter by Severity</h3>
            <div class="filter-row">
                <button class="filter-btn severity-filter-btn" data-severity="3" onclick="setSeverityFilter(3)">ERROR</button>
                <button class="filter-btn severity-filter-btn" data-severity="4" onclick="setSeverityFilter(4)">WARN</button>
                <button class="filter-btn severity-filter-btn" data-severity="6" onclick="setSeverityFilter(6)">INFO</button>
                <button class="filter-btn severity-filter-btn" data-severity="7" onclick="setSeverityFilter(7)">DEBUG</button>
            </div>

            <h3>Filter by Source</h3>
            <div class="filter-row">
                <button class="filter-btn source-filter-btn" data-source="0" onclick="setSourceFilter(0)">SYS</button>
                <button class="filter-btn source-filter-btn" data-source="1" onclick="setSourceFilter(1)">NET</button>
                <button class="filter-btn source-filter-btn" data-source="2" onclick="setSourceFilter(2)">GNSS</button>
                <button class="filter-btn source-filter-btn" data-source="3" onclick="setSourceFilter(3)">IMU</button>
            </div>
            <div class="filter-row">
                <button class="filter-btn source-filter-btn" data-source="4" onclick="setSourceFilter(4)">STEER</button>
                <button class="filter-btn source-filter-btn" data-source="5" onclick="setSourceFilter(5)">MACH</button>
                <button class="filter-btn source-filter-btn" data-source="6" onclick="setSourceFilter(6)">CAN</button>
                <button class="filter-btn source-filter-btn" data-source="8" onclick="setSourceFilter(8)">USER</button>
            </div>
        </div>

        <div class="log-container" id="logContainer">
            Loading logs...
        </div>
    </div>
</body>
</html>
)rawliteral";

#endif // TOUCH_FRIENDLY_LOG_VIEWER_PAGE_H
