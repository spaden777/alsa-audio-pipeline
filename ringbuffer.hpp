#pragma once

#include <vector>
#include <cstddef>

/*
    Simple single-producer / single-consumer ring buffer.

    Characteristics:
    - Fixed capacity
    - Non-blocking
    - Header-only template
    - Uses head/tail indices with modulo wrap

    Note:
    One slot is intentionally unused to distinguish
    between FULL and EMPTY states.

    Therefore usable capacity = (capacity - 1).
*/

template<typename T>
class RingBuffer
{
public:

    explicit RingBuffer(size_t capacity)
        : buffer(capacity),
          capacity(capacity),
          head(0),
          tail(0)
    {
    }

    bool push(const T& item)
    {
        size_t next = (head + 1) % capacity;

        if (next == tail)
            return false;   // buffer full

        buffer[head] = item;
        head = next;

        return true;
    }

    bool pop(T& item)
    {
        if (empty())
            return false;

        item = buffer[tail];
        tail = (tail + 1) % capacity;

        return true;
    }

    bool empty() const
    {
        return head == tail;
    }

    bool full() const
    {
        return ((head + 1) % capacity) == tail;
    }

    size_t size() const
    {
        if (head >= tail)
            return head - tail;

        return capacity - tail + head;
    }

    size_t max_size() const
    {
        return capacity - 1;
    }

    void clear()
    {
        head = 0;
        tail = 0;
    }

private:

    std::vector<T> buffer;

    size_t capacity;

    size_t head;
    size_t tail;
};


