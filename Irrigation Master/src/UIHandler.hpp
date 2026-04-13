#pragma once

#include <ESPAsyncWebServer.h>
#include "AppContext.hpp"

// ─────────────────────────────────────────────────────────────────────────────
// UIHandler — manages all HTTP endpoints and serves the dashboard UI
// ─────────────────────────────────────────────────────────────────────────────
class UIHandler {
public:
    // Initialize all HTTP route handlers
    static void init(AppContext &ctx);

private:
    // ── Dashboard & static assets ──────────────────────────────────────────
    static void handle_root(AsyncWebServerRequest *request);
    static void handle_dashboard(AsyncWebServerRequest *request);
    
    // ── API endpoints — status & config ────────────────────────────────────
    static void handle_get_status(AsyncWebServerRequest *request);
    static void handle_get_config(AsyncWebServerRequest *request);
    static void handle_post_config(AsyncWebServerRequest *request);
    
    // ── System info ────────────────────────────────────────────────────────
    static void handle_system_info(AsyncWebServerRequest *request);
    
    // ── Helper to get AppContext from request (if needed) ──────────────────
    static AppContext *gCtx;
};
