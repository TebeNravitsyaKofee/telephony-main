#ifndef CALL_STATE_PARSER_H
#define CALL_STATE_PARSER_H

#include <variant>
#include <optional>

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

struct AuthEvent 
{
    std::string entry_id;
    int product_id;
    int user_id;
    int64_t timestamp;
    std::string recording_id;
};

using CallEvent = std::variant<AuthEvent, CallStateEvent, RecordingEvent, CallSummary>;
static CallEvent call_state;

CallEvent parseEvent(const std::string& jsonStr);



#endif