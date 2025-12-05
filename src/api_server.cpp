#include "grid_watcher/grid_watcher.hpp"
#include "grid_watcher/web/web_server.hpp"
#include <httplib.h> // Butuh install cpp-httplib

int main() {
    auto config = gw::scada::DetectionConfig::createDefault();
    gw::scada::GridWatcher watcher(config);
    watcher.start();
    
    // HTTP Server
    httplib::Server svr;
    
    // CORS
    svr.set_pre_routing_handler([](const auto& req, auto& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        return httplib::Server::HandlerResponse::Unhandled;
    });
    
    // GET /api/statistics
    svr.Get("/api/statistics", [&](const auto& req, auto& res) {
        gw::web::GridWatcherAPI api(watcher);
        res.set_content(api.getStatistics(), "application/json");
    });
    
    // GET /api/metrics
    svr.Get("/api/metrics", [&](const auto& req, auto& res) {
        gw::web::GridWatcherAPI api(watcher);
        res.set_content(api.getMetrics(), "application/json");
    });
    
    // GET /api/blocks
    svr.Get("/api/blocks", [&](const auto& req, auto& res) {
        gw::web::GridWatcherAPI api(watcher);
        res.set_content(api.getBlockedIPs(), "application/json");
    });
    
    // POST /api/block
    svr.Post("/api/block", [&](const auto& req, auto& res) {
        // Parse JSON body untuk get IP
        std::string ip = /* parse JSON */;
        gw::web::GridWatcherAPI api(watcher);
        res.set_content(api.blockIP(ip), "application/json");
    });
    
    std::cout << "[API] Starting server on http://localhost:8080\n";
    svr.listen("0.0.0.0", 8080);
    
    watcher.stop();
    return 0;
}