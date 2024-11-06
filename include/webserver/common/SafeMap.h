#ifndef WEBSERVER_SAFE_MAP_H
#define WEBSERVER_SAFE_MAP_H

#include <mutex>
#include <unordered_map>

using std::mutex;
using std::lock_guard;

template<typename K, typename V, typename Map = std::unordered_map<K, V> >
class SafeMap {
    Map map;
    mutex m_mutex;

public:
    V &operator[](const K &key) {
        lock_guard lock(m_mutex);
        return map[key];
    }

    void insert(const K &key, const V &value) {
        lock_guard lock(m_mutex);
        map.insert({key, value});
    }

    void emplace(K &&key, V &&value) {
        lock_guard lock(m_mutex);
        map.emplace(std::move(key), std::move(value));
    }

    bool contains_key(const K &key) {
        lock_guard lock(m_mutex);
        return map.count(key) != 0;
    }


    V &get_or_default(const K &key, const V &default_value) {
        lock_guard lock(m_mutex);
        if (!contains_key(key)) {
            map.insert({key, default_value});
        }
        return map[key];
    }

    void remove(const K &key) {
        lock_guard lock(m_mutex);
        map.erase(key);
    }

    void clear() {
        lock_guard lock(m_mutex);
        map.clear();
    }
};


#endif
