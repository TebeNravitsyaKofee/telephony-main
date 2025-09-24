#ifndef CALLTRACKING_TERMINAL_H
#define CALLTRACKING_TERMINAL_H

#include <atomic>
#include <thread>
#include <nlohmann/json.hpp>
#include "../Parsers/call_state_parser.h"

using json = nlohmann::json;

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

void handlePipeMessagesThread();

bool sendJson(int fd, const json& j);
bool recvJson(int fd, json& j);


#endif