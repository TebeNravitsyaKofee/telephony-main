
#include <string.h>
#include <vector>
#include <fstream>
#include <sys/stat.h>
#include <iostream>
#include <thread>
#include <functional>
#include <atomic>
#include <unistd.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

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
    #ifdef DEBUG
    std::cout << "call_state = '" << ev.call_state << "'" << std::endl;
    std::cout << "call_id = '" << ev.call_id << "'" << std::endl;
    #endif

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
            std::string error = "Cannot open " + term + "\n";
            appendLog(error);
            #ifdef DEBUG
                std::cerr << "Cannot open " << term << std::endl;
            #endif
        }
    }

    if (!launched) 
    {
        std::string error = "Cannot open any of the terminals.\n";
        appendLog(error);
        #ifdef DEBUG
            std::cerr << "Cannot open any of the terminals." << std::endl;
        #endif
        return;
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    static std::ofstream out(fifo);
    if (!out.is_open()) 
    {
        std::string error = "Cannot open fifo for writing.\n";
        appendLog(error);
        #ifdef DEBUG
        {
            std::cerr << "Cannot open fifo for writing" << std::endl;   
        }
        #endif
        return;
    }
    static std::string buf;
    //subbing to map changes, lambda refreshes the pipe
    activeCalls.subscribe([&]() 
    {
        #ifdef DEBUG
        std::cout << "event" << std::endl;
        #endif
        std::string ret;
        for(auto const& [key,val] : activeCalls)
        {
            #ifdef DEBUG
            std::cout << val.call_state << std::endl;
            #endif
            CallStateEvent call = val;
            buf = jsonToTimestamp(val.timestamp) + " " + val.from_extension + " " + val.to_number + " " + val.call_state +  "\n";
            ret.append(buf);
        };
        //this code clears terminal
        out << "\033[2J\033[H";
        out << ret << std::flush;
    });
}

bool sendJson(int fd, const json& j) 
{
    std::string msg = j.dump();
    uint32_t len = msg.size();

    if (write(fd, &len, sizeof(len)) != sizeof(len)) 
    {
        return false;
    }

    if (write(fd, msg.data(), len) != (ssize_t)len) 
    {
        return false;
    }
    return true;
}


bool recvJson(int fd, json& j) 
{
    uint32_t len{};
    ssize_t n = read(fd, &len, sizeof(len));
    if (n == 0) return false;
    if (n != sizeof(len)) return false;

    std::string buf(len, '\0');
    size_t total = 0;
    while (total < len) 
    {
        ssize_t r = read(fd, &buf[total], len - total);
        if (r <= 0) return false;
        total += r;
    }

    j = json::parse(buf);
    return true;
}

void handlePipeMessagesThread() 
{
    while (pipe_thread_running) 
    {
        json j;
        if (!recvJson(pipe_fd[0], j)) 
        {
            continue;
        }

        std::string type = j.value("msg_type", "");
        if (type == "U") 
        {
            CallStateEvent ev;
            ev.entry_id = j.value("entry_id", "");
            ev.call_id = j.value("call_id", "");
            ev.timestamp = j.value("timestamp", 0);
            ev.seq = j.value("seq", 0);
            ev.call_state = j.value("call_state", "");
            ev.location = j.value("location", "");
            ev.from_extension = j.value("from_extension", "");
            ev.from_number = j.value("from_number", "");
            ev.from_line_number = j.value("from_line_number", "");
            ev.to_number = j.value("to_number", "");
            ev.sip_call_id = j.value("sip_call_id", "");

            int dr = j.value("disconnect_reason", -1);
            if (dr != -1) ev.disconnect_reason = dr;

            int dct = j.value("dct_type", -1);
            if (dct != -1) ev.dct_type = dct;

            storeCallState(ev);
        } 
        else if (type == "R") 
        {
            std::string call_id = j.value("call_id", "");
            activeCalls.erase(call_id);
        } 
        else if (type == "C") 
        {
            activeCalls.clear();
        }
    }
}

void stopPipeThread() 
{
    pipe_thread_running = false;
    if (pipe_thread.joinable()) 
    {
        pipe_thread.join();
    }
}

