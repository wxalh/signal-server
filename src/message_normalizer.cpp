#include "message_normalizer.h"

#include <nlohmann/json.hpp>

std::string normalizeRemoteEndpointAddress(const std::string& remoteEndpoint)
{
    if (remoteEndpoint.empty()) return {};
    if (remoteEndpoint.front() == '[') {
        const auto closing = remoteEndpoint.find(']');
        if (closing != std::string::npos) return remoteEndpoint.substr(1, closing - 1);
    }

    const auto firstColon = remoteEndpoint.find(':');
    const auto lastColon = remoteEndpoint.rfind(':');
    if (firstColon != std::string::npos && firstColon == lastColon) {
        return remoteEndpoint.substr(0, firstColon);
    }
    return remoteEndpoint;
}

NormalizedForwardMessage normalizeForwardedMessage(const std::string& payload,
                                                    const std::string& sessionId,
                                                    const std::string& remoteEndpoint)
{
    NormalizedForwardMessage result;
    if (sessionId.empty()) {
        result.error = "missing authenticated session id";
        return result;
    }

    auto object = nlohmann::json::parse(payload, nullptr, false);
    if (!object.is_object()) {
        result.error = "invalid JSON object";
        return result;
    }

    const auto receiver = object.find("receiver");
    if (receiver == object.end() || !receiver->is_string() || receiver->get_ref<const std::string&>().empty()) {
        result.error = "missing receiver";
        return result;
    }
    const auto type = object.find("type");
    if (type == object.end() || !type->is_string() || type->get_ref<const std::string&>().empty()) {
        result.error = "missing type";
        return result;
    }

    object["sender"] = sessionId;
    if (type->get_ref<const std::string&>() == "connect") {
        object["source_ip"] = normalizeRemoteEndpointAddress(remoteEndpoint);
    }

    result.valid = true;
    result.receiver = receiver->get<std::string>();
    result.payload = object.dump();
    return result;
}
