#include "ReStreamAgent.h"

#include <CxxPtr/GlibPtr.h>

#include "Client/WsClient.h"

#include "GstStreaming/GstTestStreamer.h"
#include "GstStreaming/GstReStreamer.h"

#include "Log.h"
#include "Session.h"


enum {
    DEFAULT_RECONNECT_TIMEOUT = 5,
};

static const auto Log = AgentLog;

static std::unique_ptr<WebRTCPeer>
CreatePeer(
    const Config* config,
    const std::string& uri)
{
    auto streamerIt = config->streamers.end();

    if(uri == config->name) {
        streamerIt = config->streamers.find(std::string());
    } else if(uri.size() > config->name.size() + 1 &&
       0 == uri.compare(0, config->name.size(), config->name) &&
       uri[config->name.size()] == '/')
    {
        const std::string streamerName = uri.substr(config->name.size() + 1);
        streamerIt = config->streamers.find(streamerName);
    }

    if(config->streamers.end() == streamerIt) {
        Log()->error("Unknown URI \"{}\"", uri);
        return std::unique_ptr<WebRTCPeer>();
    }

    const StreamerConfig& streamer = streamerIt->second;

    switch(streamer.type) {
    case StreamerConfig::Type::Test:
        return std::make_unique<GstTestStreamer>(streamer.uri);
    case StreamerConfig::Type::ReStreamer:
        return std::make_unique<GstReStreamer>(streamer.uri);
    default:
        return std::unique_ptr<WebRTCPeer>();
    }
}

static std::unique_ptr<rtsp::ServerSession> CreateSession (
    const Config* config,
    Session::Cache* cache,
    const std::function<void (const rtsp::Request*) noexcept>& sendRequest,
    const std::function<void (const rtsp::Response*) noexcept>& sendResponse) noexcept
{
    return
        std::make_unique<Session>(
            config,
            cache,
            std::bind(CreatePeer, config, std::placeholders::_1),
            sendRequest, sendResponse);
}

static void ClientDisconnected(
    const Config* config,
    client::WsClient* client) noexcept
{
    const unsigned reconnectTimeout =
        config->reconnectTimeout > 0 ?
            config->reconnectTimeout :
            DEFAULT_RECONNECT_TIMEOUT;
    GSourcePtr timeoutSourcePtr(g_timeout_source_new_seconds(reconnectTimeout));
    GSource* timeoutSource = timeoutSourcePtr.get();
    g_source_set_callback(timeoutSource,
        [] (gpointer userData) -> gboolean {
            static_cast<client::WsClient*>(userData)->connect();
            return false;
        }, client, nullptr);
    g_source_attach(timeoutSource, g_main_context_get_thread_default());
}

int ReStreamAgentMain(const Config& config)
{
    GMainContextPtr clientContextPtr(g_main_context_new());
    GMainContext* clientContext = clientContextPtr.get();
    g_main_context_push_thread_default(clientContext);
    GMainLoopPtr loopPtr(g_main_loop_new(clientContext, FALSE));
    GMainLoop* loop = loopPtr.get();

    Session::Cache sessionsCache;

    client::WsClient client(
        config.clientConfig,
        loop,
        std::bind(
            CreateSession,
            &config,
            &sessionsCache,
            std::placeholders::_1,
            std::placeholders::_2),
        std::bind(ClientDisconnected, &config, &client));

    if(client.init()) {
        client.connect();
        g_main_loop_run(loop);
    } else
        return -1;

    return 0;
}
