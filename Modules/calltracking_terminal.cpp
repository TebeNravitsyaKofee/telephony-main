
#include <string.h>
#include <vector>
#include <fstream>
#include <sys/stat.h>
#include <iostream>
#include <thread>
#include <functional>
#include <atomic>
#include <unistd.h>

#include "../settings.h"
#include "../Parsers/call_state_parser.h"

std::atomic<bool> pipe_thread_running{true};
std::thread pipe_thread;
int pipe_fd[2];

//custom observable map for in-real-time calltracking, emits signals when edited
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

//map for storing active calls
ObservableMap<std::string, CallStateEvent> activeCalls;



//function that parses calls (only CallStateEvent) to map and controls call state flow
void storeCallState(const CallStateEvent& ev) 
{
    std::cout << "DEBUG: call_state = '" << ev.call_state << "'" << std::endl;
    std::cout << "DEBUG: call_id = '" << ev.call_id << "'" << std::endl;

    if (ev.call_state == "Appeared") 
    {
        activeCalls.insertOrUpdate(ev.call_id,ev);
    }
    else if (ev.call_state == "Connected") 
    {
        auto it = activeCalls.find(ev.call_id);
        if (it != activeCalls.end()) 
        {
            activeCalls.insertOrUpdate(ev.call_id,ev);
        }
        //in some cases second call state can come earlier than first
        //if call not found i just add it in for now, gotta rework that later
        else 
        {
            activeCalls.insertOrUpdate(ev.call_id,ev);
        }
    }
    else if (ev.call_state == "Disconnected") 
    {
        auto it = activeCalls.find(ev.call_id);
        //here i will search for call_summary and add it to posgresql
        if (it != activeCalls.end()) 
        {
            #ifdef DEBUG
            std::cout << "Звонок завершён: " << ev.call_id 
                      << " seq=" << ev.seq 
                      << " timestamp=" << ev.timestamp << "\n";
            #endif

            activeCalls.erase(it);
        }
    }
}

//method looks for installed terminals, made for a bit of versatility
bool isInstalled(const std::string &cmd) 
{
    std::string check = "which " + cmd + " > /dev/null 2>&1";
    int ret = system(check.c_str());
    return WIFEXITED(ret) && WEXITSTATUS(ret) == 0;
}

//runs terminal with a dedicated script, might be reused
bool tryRunTerminal(const std::string &terminal, const std::string &scriptPath) 
{
    std::string cmd;
    if (terminal == "gnome-terminal") 
    {
        cmd = terminal + " -- " + scriptPath;
    } 
    else if (terminal == "konsole") 
    {
        cmd = terminal + " -e " + scriptPath;
    } 
    else if (terminal == "xterm") 
    {
        cmd = terminal + " -e " + scriptPath;
    } 
    else if (terminal == "terminator") 
    {
        cmd = terminal + " -x " + scriptPath;
    } 
    else 
    {
        return false;
    }
    return system(cmd.c_str()) == 0;
}

void displayCalls() 
{
    //creating pipe to deliver data inside newly opened terminal
    const char* fifo = "/tmp/my_pipe";
    mkfifo(fifo, 0666);

    const char* scriptPath = "/tmp/show_table.sh";
    std::ofstream script(scriptPath);
    script << "#!/bin/bash\n"
           << "while true; do\n"
           << "    clear\n"
           << "    cat /tmp/my_pipe\n"
           << "    sleep 1\n"
           << "done\n";
    script.close();
    //groups and others cannot write
    chmod(scriptPath, 0755);

    std::vector<std::string> terminals = {"gnome-terminal", "konsole", "xterm", "terminator"};

    bool launched = false;
    for (const auto &term : terminals) 
    {
        if (!isInstalled(term)) continue;

        if (tryRunTerminal(term, scriptPath)) 
        {
            launched = true;
            break;
        } 
        else 
        {
            std::string error = "Не удалось запустить " + term + ". Переходим к следующему.\n";
            appendLog(error);
            #ifdef DEBUG
                std::cerr << "Не удалось запустить " << term << ". Переходим к следующему." << std::endl;
            #endif
        }
    }

    if (!launched) 
    {
        std::string error = "Не удалось запустить ни один терминал.\n";
        appendLog(error);
        #ifdef DEBUG
            std::cerr << "Не удалось запустить ни один терминал." << std::endl;
        #endif
        return;
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    static std::ofstream out(fifo);
    if (!out.is_open()) 
    {
        std::string error = "Не удалось открыть FIFO на запись.\n";
        appendLog(error);
        #ifdef DEBUG
        {
            std::cerr << "Не удалось открыть FIFO на запись" << std::endl;   
        }
        #endif
        return;
    }
    static std::string buf;
    //subbing to map changes, lambda refreshes the pipe
    activeCalls.subscribe([&]() 
    {
        std::cout << "event" << std::endl;
        std::string ret;
        for(auto const& [key,val] : activeCalls)
        {
            std::cout << val.call_state << std::endl;
            CallStateEvent call = val;
            buf = val.from_number + " " + val.call_state + " " + val.location + "\n";
            ret.append(buf);
            

        };
        //this code clears terminal
        out << "\033[2J\033[H";
        out << ret << std::flush;
    });
    
    /*
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
        return ev; */
}

//struct for transfering data from server process to parent
struct PipeMessage 
{
    char type; //'U' - update, 'R' - remove, 'C' - clear
    char call_id[65]; //+1 for null term
    CallStateEvent event;
    
    //consctructor is needed to initialise "clear" pipe message before filling it with data
    PipeMessage() : type(' ') 
    {
        memset(call_id, 0, sizeof(call_id));
    }
};


void processPipeMessage(const PipeMessage& msg) 
{
    #ifdef DEBUG
    if (msg.call_id[0] == '\0') 
    {
        std::cout << "ERROR: Empty call_id in pipe message" << std::endl;
        return;
    }
    
    std::string call_id(msg.call_id);
    
    if (msg.event.call_state.empty()) {
        std::cout << "ERROR: Empty call_state for call_id: " << call_id << std::endl;
        return;
    }
    
    std::cout << "PROCESSING: call_id = '" << call_id 
              << "', call_state = '" << msg.event.call_state << "'" << std::endl;
    #endif
    
    switch (msg.type) 
    {
        case 'U': //update
            storeCallState(msg.event);
            break;
            
        case 'R': //remove
            activeCalls.erase(call_id);
            break;
            
        case 'C': //clear all
            activeCalls.clear();
            break;
            
        default:
            std::cout << "Unknown message type: '" << msg.type << "'" << std::endl;
    }
}

//turns out pipes cant work with strings and optionals, this serialized data for it to go throuhg pipe correctly
//CallStateEvent has strings and optionals, while pipe can only send chars safely
//this scruct makes sure data is serialized properly
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

void handlePipeMessagesThread() 
{
    SerializableCallStateEvent serial;
    
    while (pipe_thread_running) 
    {
        memset(&serial, 0, sizeof(serial));
        
        ssize_t bytes_read = read(pipe_fd[0], &serial, sizeof(serial));
        
        if (bytes_read == sizeof(serial)) 
        {
            std::cout << "RECEIVED: type = '" << serial.type 
                      << "', call_id = '" << serial.call_id 
                      << "', call_state = '" << serial.call_state << "'" << std::endl;
            
            if (serial.type == 'U' && serial.call_id[0] != '\0') 
            {
                CallStateEvent ev = serial.toCallStateEvent();
                storeCallState(ev);
            }
        }
        else 
        {
            appendLog("Incomplete pipe message: " + std::to_string(bytes_read) + "/" + 
                     std::to_string(sizeof(serial)) + " bytes");
        }
    }
}

void stopPipeThread() 
{
    pipe_thread_running = false;
    if (pipe_thread.joinable()) {
        pipe_thread.join();
    }
}