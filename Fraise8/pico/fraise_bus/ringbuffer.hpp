/**
 * Copyright (c) 2026 metalu.net
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <vector>

template <typename T, int capacity> class RingBuffer {
private:
    //std::vector<T> buffer;
    T buffer[capacity];
    int front = 0;
    int back = 0;

public:

    //RingBuffer() : buffer(capacity) {}

    void clear() {
        front = back = 0;
    }

    // Function to add an element to the buffer
    void push_back(int val) {
        if (full()) {
            return;
        }
        buffer[back] = val;
        back = (back + 1) % capacity;
    }

    // Function to remove an element from the buffer
    void pop_front() {
        if (empty()) {
            return;
        }
        front = (front + 1) % capacity;
    }
    
    T getFront() {
        /*if (empty()) {
            
        }*/
        return buffer[front];
    }
    
    T getBack() {
        if (empty()) {
            //throw out_of_range("CircularBuffer is empty");
            return buffer[0];
        }
        return (back == 0) ? buffer[capacity - 1] : buffer[back - 1];
    }

    // Function to check if the buffer is empty
    bool empty() const { return front == back; }

    // Function to check if the buffer is full
    bool full() const {
        return (back + 1) % capacity == front;
    }

    // Function to get the size of the buffer
    int size() const {
        if (back >= front) {
            return back - front;
        }
        return capacity - (front - back);
    }

    int available() const {
        return MAX(capacity - size() - 1, 0);
    }

    bool queue_message(const char *data, int len) {
        if(available() < len + 1) {
            return false;
        }
        push_back(len + 128);
        for(int i = 0; i < len; i++) {
            push_back(data[i] & 127);
        }
        return true;
    }

    int unqueue_message(char *data) {
        if(empty()) {
            return 0;
        }
        char l = getFront();
        if(!(l & 128)) { // format error in message queue!
            while(!empty()) {
                if(getFront() & 128) break;
                pop_front();
            }
            if(empty()) {
                return 0;
            }
            l = getFront();
        }
        pop_front();
        l &= 127;
        for(int i = 0; i < l; i++) {
            if(empty()) { // empty error in message queue!
                return 0;
            }
            data[i] = getFront();
            if(data[i] & 128) { // premature end error in message queue!
                return 0;
            }
            pop_front();
        }
        return l;
    }
};

