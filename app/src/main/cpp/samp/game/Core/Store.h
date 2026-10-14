/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

template <typename ObjectType, size_t Capacity> class CStore {
public:
    size_t     m_nCount;
    ObjectType m_aObjects[Capacity];

public:
    CStore() {
        m_nCount = 0;
    }

    ObjectType& GetItemAtIndex(size_t index) {
        return m_aObjects[index];
    }

    // Capacity queries (added for 0.3.DL custom model safety).
    constexpr size_t GetCapacity() const { return Capacity; }
    size_t GetCount() const { return m_nCount; }
    bool IsFull() const { return m_nCount >= Capacity; }
    size_t GetRemaining() const { return IsFull() ? 0 : (Capacity - m_nCount); }

    // Safe variant: returns nullptr instead of writing out of bounds.
    // Prefer this over AddItem() when the store may be full.
    inline ObjectType* TryAddItem() {
        if (m_nCount >= Capacity) {
            return nullptr;
        }
        auto& obj = m_aObjects[m_nCount];
        ++m_nCount;
        return &obj;
    }

    inline ObjectType& AddItem() {
        // Bounds check: never write past the end of m_aObjects.
        // If full, return a static dummy instead of corrupting the heap.
        // Callers that need to detect this should use TryAddItem()/IsFull().
        if (m_nCount >= Capacity) {
            static ObjectType dummy{};
            return dummy;
        }
        auto& obj = m_aObjects[m_nCount];
        ++m_nCount;
        return obj;
    }
};
