#ifndef CALLTRACKING_TERMINAL_H
#define CALLTRACKING_TERMINAL_H

#include <atomic>
#include <thread>
#include "../Parsers/call_state_parser.h"

void displayCalls();
void storeCallState(const CallStateEvent& ev);
void stopPipeThread();

extern std::atomic<bool> pipe_thread_running;
extern std::thread pipe_thread;
extern int pipe_fd[2];

template<typename K, typename V>
class ObservableMap 
{
    std::map<K, V> data;

public:
    using Listener = std::function<void()>;

private:
    std::vector<Listener> listeners;

    void notify() 
    {
        for (auto &fn : listeners) fn();
    }

public:
    //subscribing for notifying
    void subscribe(Listener fn) 
    {

        listeners.push_back(fn);
    }
    
    //standart methods for map
    void insertOrUpdate(const K& key, const V& value) 
    {
        data[key] = value;
        notify();
    }

    //erase for every possible way to use it
    void erase(const K& key)
    {
        auto it = data.find(key);
        if (it != data.end()) 
        {
            data.erase(it);
            notify();
        }
    }
    void erase(typename std::map<K, V>::iterator pos) 
    {
        data.erase(pos);
        notify();
    }
    void erase(typename std::map<K, V>::const_iterator pos) 
    {
        data.erase(pos);
        notify();
    }

    void erase(typename std::map<K, V>::iterator first, typename std::map<K, V>::iterator last) 
    {
        if (first != last) 
        {
            data.erase(first, last);
            notify();
        }
    }

    //access like in std::map
    V& operator[](const K& key) 
    {
        notify(); 
        return data[key];
    }

    V& at(const K& key) 
    {
    return data.at(key);
    }
    const V& at(const K& key) const 
    {
        return data.at(key);
    }

    auto clear() { return data.clear(); }

    std::size_t size() const { return data.size(); }
    bool empty() const { return data.empty(); }

    bool operator==(const ObservableMap& other) const 
    {
        return data == other.data;
    }
    bool operator!=(const ObservableMap& other) const 
    {
        return data != other.data;
    }

    auto find(const K& key) { return data.find(key); }
    auto find(const K& key) const { return data.find(key); }

    auto begin() const { return data.begin(); }
    auto end() const { return data.end(); }
};

extern ObservableMap<std::string, CallStateEvent> activeCalls;

struct SerializableCallStateEvent 
{
    char type; //'U' - update, 'R' - remove
    char call_id[65];
    char entry_id[65];
    char call_state[20];
    char from_extension[20];
    char from_number[20];
    char from_line_number[20];
    char to_number[20];
    char sip_call_id[100];
    int64_t timestamp;
    int seq;
    int disconnect_reason; //-1 if empty
    int dct_type; //-1 if empty
    
    //constructor that prepares memory with memset
    SerializableCallStateEvent() : type(' '), timestamp(0), seq(0), 
                                  disconnect_reason(-1), dct_type(-1) 
    {
        memset(call_id, 0, sizeof(call_id));
        memset(entry_id, 0, sizeof(entry_id));
        memset(call_state, 0, sizeof(call_state));
        memset(from_extension, 0, sizeof(from_extension));
        memset(from_number, 0, sizeof(from_number));
        memset(from_line_number, 0, sizeof(from_line_number));
        memset(to_number, 0, sizeof(to_number));
        memset(sip_call_id, 0, sizeof(sip_call_id));
    }
    
    //converter from
    static SerializableCallStateEvent fromCallStateEvent(const CallStateEvent& ev, char msg_type = 'U') 
    {
        SerializableCallStateEvent serial;
        serial.type = msg_type;
        
        //copying strings with length check
        copyString(serial.call_id, ev.call_id, sizeof(serial.call_id));
        copyString(serial.entry_id, ev.entry_id, sizeof(serial.entry_id));
        copyString(serial.call_state, ev.call_state, sizeof(serial.call_state));
        copyString(serial.from_extension, ev.from_extension, sizeof(serial.from_extension));
        copyString(serial.from_number, ev.from_number, sizeof(serial.from_number));
        copyString(serial.from_line_number, ev.from_line_number, sizeof(serial.from_line_number));
        copyString(serial.to_number, ev.to_number, sizeof(serial.to_number));
        copyString(serial.sip_call_id, ev.sip_call_id, sizeof(serial.sip_call_id));
        
        serial.timestamp = ev.timestamp;
        serial.seq = ev.seq;
        
        //optionals
        serial.disconnect_reason = ev.disconnect_reason.value_or(-1);
        serial.dct_type = ev.dct_type.value_or(-1);
        
        return serial;
    }
    
    //converting to
    CallStateEvent toCallStateEvent() const 
    {
        CallStateEvent ev;
        ev.call_id = call_id;
        ev.entry_id = entry_id;
        ev.call_state = call_state;
        ev.from_extension = from_extension;
        ev.from_number = from_number;
        ev.from_line_number = from_line_number;
        ev.to_number = to_number;
        ev.sip_call_id = sip_call_id;
        ev.timestamp = timestamp;
        ev.seq = seq;
        
        if (disconnect_reason != -1) 
        {
            ev.disconnect_reason = disconnect_reason;
        }
        if (dct_type != -1) 
        {
            ev.dct_type = dct_type;
        }
        
        return ev;
    }
    
private:
    //this always sets last byte to null terminator
    static void copyString(char* dest, const std::string& src, size_t dest_size) 
    {
        strncpy(dest, src.c_str(), dest_size - 1);
        dest[dest_size - 1] = '\0';
    }
};


void handlePipeMessagesThread();


#endif