#pragma once

/**
 * @brief Generic Node for EffectQueue
 */
template <typename T>
struct EffectNode {
    T data;
    EffectNode* next;
};

/**
 * @brief A generic FIFO queue for visual effects (Damage popups, Message logs, etc.)
 * Implemented as a linked list to match the project's existing style.
 */
template <typename T>
class EffectQueue {
public:
    /** @brief Constructor */
    EffectQueue() : head(nullptr), tail(nullptr), count(0) {}
    
    /** @brief Destructor */
    ~EffectQueue() {
        Clear();
    }

    /** @brief Tambah data ke antrian */
    void Enqueue(T data) {
        EffectNode<T>* newNode = new EffectNode<T>{data, nullptr};
        if (tail == nullptr) {
            head = tail = newNode;
        } else {
            tail->next = newNode;
            tail = newNode;
        }
        count++;
    }

    /** @brief Hapus data dari depan antrian */
    void Dequeue() {
        if (head == nullptr) return;

        EffectNode<T>* temp = head;
        head = head->next;
        if (head == nullptr) {
            tail = nullptr;
        }
        delete temp;
        count--;
    }

    /** @brief Cek apakah antrian kosong */
    bool IsEmpty() const {
        return head == nullptr;
    }

    /** @brief Kosongkan seluruh antrian */
    void Clear() {
        while (!IsEmpty()) {
            Dequeue();
        }
    }

    /** @brief Ambil jumlah data dalam antrian */
    int Size() const {
        return count;
    }

    /** @brief Ambil node paling depan */
    EffectNode<T>* GetHead() const {
        return head;
    }

private:
    EffectNode<T>* head;
    EffectNode<T>* tail;
    int count;
};
