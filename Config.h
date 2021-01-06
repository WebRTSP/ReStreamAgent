#pragma once

#include "Client/Config.h"

#include <string>
#include <map>

#include <spdlog/common.h>


struct StreamerConfig
{
    enum class Type {
        Test,
        ReStreamer,
    };

    Type type;
    std::string uri;
    std::string description;
};

struct Config
{
    spdlog::level::level_enum logLevel = spdlog::level::info;
    spdlog::level::level_enum lwsLogLevel = spdlog::level::warn;

    client::Config clientConfig;
    unsigned reconnectTimeout;
    std::string name;
    std::string description;
    std::string authToken;

    std::map<std::string, StreamerConfig> streamers;
};
