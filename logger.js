import { CONFIG } from './config.js';

export class Logger {
    static _entryCount = 0;

    static log(level, context, message) {
        if (level < CONFIG.LOGGING.CURRENT_LEVEL) return;

        const time = new Date().toLocaleTimeString();
        let color = '#e9edef';
        let prefix = '[INFO]';

        switch (level) {
            case CONFIG.LOGGING.LEVELS.DEBUG:
                color = '#8696a0'; prefix = '[DEBUG]'; break;
            case CONFIG.LOGGING.LEVELS.INFO:
                color = '#00a884'; prefix = '[INFO]'; break;
            case CONFIG.LOGGING.LEVELS.WARN:
                color = '#ffeb3b'; prefix = '[WARN]'; break;
            case CONFIG.LOGGING.LEVELS.ERROR:
                color = '#ef9a9a'; prefix = '[ERROR]'; break;
        }

        const consoleEl = document.getElementById('console');
        if (consoleEl) {
            // Enforce max entries to prevent unbounded DOM growth (O(n²) fix)
            while (this._entryCount >= CONFIG.LOGGING.MAX_LOG_ENTRIES) {
                const first = consoleEl.firstChild;
                if (first) { consoleEl.removeChild(first); this._entryCount--; }
                else break;
            }

            // Use createElement instead of innerHTML += to avoid XSS and DOM reparse
            const entry = document.createElement('div');

            const timeSpan = document.createElement('span');
            timeSpan.style.color = '#555';
            timeSpan.textContent = `[${time}]`;

            const prefixSpan = document.createElement('span');
            prefixSpan.style.color = '#aaa';
            prefixSpan.textContent = ` ${prefix} [${context}] `;

            const msgSpan = document.createElement('span');
            msgSpan.style.color = color;
            msgSpan.textContent = message;

            entry.appendChild(timeSpan);
            entry.appendChild(prefixSpan);
            entry.appendChild(msgSpan);

            consoleEl.appendChild(entry);
            this._entryCount++;

            // Only auto-scroll if near bottom to allow reading history
            if (consoleEl.scrollHeight - consoleEl.scrollTop - consoleEl.clientHeight < 50) {
                consoleEl.scrollTop = consoleEl.scrollHeight;
            }
        }

        console.log(`${prefix} [${context}] ${message}`);
    }

    static debug(context, message) { this.log(CONFIG.LOGGING.LEVELS.DEBUG, context, message); }
    static info(context, message) { this.log(CONFIG.LOGGING.LEVELS.INFO, context, message); }
    static warn(context, message) { this.log(CONFIG.LOGGING.LEVELS.WARN, context, message); }
    static error(context, message) { this.log(CONFIG.LOGGING.LEVELS.ERROR, context, message); }

    static clear() {
        const consoleEl = document.getElementById('console');
        if (consoleEl) {
            consoleEl.innerHTML = '';
            this._entryCount = 0;
        }
    }
}
