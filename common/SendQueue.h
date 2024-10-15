//
// Created by Haotian on 2024/10/15.
//

#ifndef SEND_QUEUE_H
#define SEND_QUEUE_H
#include <vector>

template<class T>
class SendQueue {
    std::vector<T> m_data{};
    // Point to the next available data index
    int m_next_data_idx = 0;
    int m_ready_to_send_idx = 0;

public:
    T &get_next_data() {
        return m_data[m_next_data_idx];
    }

    void move_next_data() {
        if (m_next_data_idx >= m_ready_to_send_idx) return;
        m_next_data_idx++;
    }

    void add_data(T &data) {
        m_data.push_back(data);
    }

    void add_range(const std::vector<T> &data) {
        m_data.insert(m_data.begin(), data.begin(), data.end());
    }

    void commit() {
        m_ready_to_send_idx = m_data.size();
    }

    void clear() {
        m_data.clear();
        m_next_data_idx = 0;
        m_ready_to_send_idx = 0;
    }

    [[nodiscard]] bool empty() const {
        return m_next_data_idx >= m_ready_to_send_idx;
    }

    [[nodiscard]] bool has_uncommitted_data() const {
        return m_ready_to_send_idx < m_data.size();
    }
};

#endif //SEND_QUEUE_H
