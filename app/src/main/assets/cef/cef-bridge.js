/*!
 * SA-MP Mobile CEF bridge (omp-cef compatible).
 *
 * On PC, omp-cef injects window.cef (on/off/emit/set_focus/isChatInputOpen/exitGame)
 * into every page via a V8 extension. On Android the launcher injects the same API
 * natively before page scripts run, so this file is only a safety net:
 * if window.cef is missing (e.g. the page is opened in a plain browser), a
 * console-logging fallback is installed so the UI never crashes.
 *
 * Usage in your UI: <script src="cef-bridge.js"></script>
 */
(function () {
    'use strict';

    function fallbackLog(type, name, args) {
        if (window.console && console.log) {
            console.log('[CEF cef.' + type + ']', name, args || []);
        }
    }

    if (!window.cef) {
        window.cef = {
            __sampMobile: false,
            on: function (name, callback) { fallbackLog('on', name); },
            off: function (name, callback) { fallbackLog('off', name); },
            emit: function (name) {
                var args = Array.prototype.slice.call(arguments, 1);
                fallbackLog('emit', name, args);
            },
            set_focus: function (focused) { fallbackLog('set_focus', focused); },
            isChatInputOpen: function () { return false; },
            exitGame: function () { fallbackLog('exitGame'); }
        };
    }

    // Convenience namespace mirroring the omp-cef demo.
    window.CefDemo = window.CefDemo || {};
    window.CefDemo.on = function (name, callback) {
        if (!window.cef || typeof window.cef.on !== 'function') { fallbackLog('on', name); return; }
        window.cef.on(name, callback);
    };
    window.CefDemo.emit = function (name) {
        var args = Array.prototype.slice.call(arguments, 1);
        if (!window.cef || typeof window.cef.emit !== 'function') { fallbackLog('emit', name, args); return; }
        window.cef.emit.apply(window.cef, [name].concat(args));
    };
})();
