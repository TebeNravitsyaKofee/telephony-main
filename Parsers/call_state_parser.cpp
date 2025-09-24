#include <nlohmann/json.hpp>
#include <string.h>
#include <variant>
#include <optional>
#include <iostream>


using json = nlohmann::json;

std::string jsonToTimestamp(const json& j) 
{
    if (j.is_null()) return "";

    long long raw{};
    if (j.is_number()) 
    {
        raw = j.get<long long>();
    } else 
    {
        return "";
    }
    time_t ts = static_cast<time_t>(raw);

    std::tm tm{};
    gmtime_r(&ts, &tm);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S+00");
    return oss.str();
}



//data structures that store /call events
struct AuthEvent 
{
    std::string entry_id;
    int product_id;
    int user_id;
    int64_t timestamp;
    std::string recording_id;
};

struct CallStateEvent 
{
    std::string entry_id;
    std::string call_id;
    int64_t timestamp;
    int seq;
    std::string call_state; //Appeared, Connected, Disconnected
    std::string location;

    std::string from_extension;
    std::string from_number;
    std::string from_line_number;

    std::string to_number;

    std::optional<int> disconnect_reason; //только для Disconnected
    std::optional<int> dct_type; //Appeared, Connected, Disconnected
    std::string sip_call_id;
};

struct RecordingEvent 
{
    std::string recording_id;
    std::string recording_state; // Started, Completed, Stopped
    int seq;
    std::string entry_id;
    std::string call_id;
    std::string extension;
    int64_t timestamp;
    std::string recipient;

    std::optional<int> completion_code;
};

struct CallSummary 
{
    std::string entry_id;
    int call_direction; //1 - incoming, 2 - outgoing
    std::string from_extension;
    std::string from_number;
    std::string to_number;
    std::string line_number;

    std::string create_time;
    std::string forward_time;
    std::string talk_time;
    std::string end_time;

    int entry_result;
    int disconnect_reason;
    std::string sip_call_id;
};

using CallEvent = std::variant<AuthEvent, CallStateEvent, RecordingEvent, CallSummary>;
static CallEvent call_state;

//this map stores every /call event before it being redirected, not needed in current state
//std::map<std::string, CallEvent> events;

//this function distributes events to data structures
CallEvent parseEvent(const std::string& jsonStr) 
{
    auto j = nlohmann::json::parse(jsonStr);

    if (j.contains("call_state")) 
    {
        CallStateEvent ev;
        ev.entry_id = j.value("entry_id", "");
        ev.call_id = j.value("call_id", "");
        ev.timestamp = j.value("timestamp", 0);
        ev.seq = j.value("seq", 0);
        ev.call_state = j.value("call_state", "");
        ev.location = j.value("location", "");
        if (j.contains("from")) 
        {
            ev.from_extension = j["from"].value("extension", "");
            ev.from_number = j["from"].value("number", "");
            ev.from_line_number = j["from"].value("line_number", "");
        }
        if (j.contains("to")) 
        {
            ev.to_number = j["to"].value("number", "");
        }
        if (j.contains("disconnect_reason"))
            ev.disconnect_reason = j["disconnect_reason"].get<int>();
        if (j.contains("dct"))
            ev.dct_type = j["dct"].value("type", 0);
        ev.sip_call_id = j.value("sip_call_id", "");
        return ev;
    }
    else if (j.contains("recording_state")) 
    {
        RecordingEvent ev;
        ev.recording_id = j.value("recording_id", "");
        ev.recording_state = j.value("recording_state", "");
        ev.seq = j.value("seq", 0);
        ev.entry_id = j.value("entry_id", "");
        ev.call_id = j.value("call_id", "");
        ev.extension = j.value("extension", "");
        ev.timestamp = j.value("timestamp", 0);
        ev.recipient = j.value("recipient", "");
        if (j.contains("completion_code"))
            ev.completion_code = j["completion_code"].get<int>();
        return ev;
    }
    else if (j.contains("call_direction")) 
    {
        CallSummary ev;
        ev.entry_id = j.value("entry_id", "");
        ev.call_direction = j.value("call_direction", 0);
        if (j.contains("from")) 
        {
            ev.from_extension = j["from"].value("extension", "");
            ev.from_number = j["from"].value("number", "");
        }
        if (j.contains("to")) 
        {
            ev.to_number = j["to"].value("number", "");
        }
        ev.line_number = j.value("line_number", "");
        ev.create_time = jsonToTimestamp(j["create_time"]);
        ev.forward_time = jsonToTimestamp(j["forward_time"]);
        ev.talk_time = jsonToTimestamp(j["talk_time"]);
        ev.end_time = jsonToTimestamp(j["end_time"]);
        ev.entry_result = j.value("entry_result", 0);
        ev.disconnect_reason = j.value("disconnect_reason", 0);
        ev.sip_call_id = j.value("sip_call_id", "");
        return ev;
    }
    else if (j.contains("product_id")) 
    {
        AuthEvent ev;
        ev.entry_id = j.value("entry_id", "");
        ev.product_id = j.value("product_id", 0);
        ev.user_id = j.value("user_id", 0);
        ev.timestamp = j.value("timestamp", 0);
        ev.recording_id = j.value("recording_id", "");
        return ev;
    }
    else if (j.contains("key")) 
    {
        return {};
    }

    throw std::runtime_error("Unknown event type");
}