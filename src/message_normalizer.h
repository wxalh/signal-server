#pragma once

#include <string>

struct NormalizedForwardMessage {
    bool valid{false};
    std::string receiver;
    std::string payload;
    std::string error;
};

std::string normalizeRemoteEndpointAddress(const std::string& remoteEndpoint);
NormalizedForwardMessage normalizeForwardedMessage(const std::string& payload,
                                                    const std::string& sessionId,
                                                    const std::string& remoteEndpoint);
