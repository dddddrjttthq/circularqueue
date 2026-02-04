/**
 * @file circularqueue.h
 * @brief 环形队列实现，用于CAN消息缓存
 * @details 针对CAN通信优化的无锁环形队列，避免频繁内存分配
 */

#pragma once

#include <atomic>
#include <vector>
#include <cstddef>  // 为size_t类型

template<typename T>
class CircularQueue {
public:
    /**
     * @brief 构造函数
     * @param capacity 队列容量（必须是2的幂）
     */
    explicit CircularQueue(size_t capacity = 4096)
        : m_capacity(capacity)
        , m_mask(capacity - 1)
        , m_buffer(capacity)
        , m_writeIndex(0)
        , m_readIndex(0)
    {
        // 确保容量是2的幂，便于使用位操作优化
        if ((capacity & (capacity - 1)) != 0) {
            // 如果不是2的幂，调整到最接近的2的幂
            size_t powerOf2 = 1;
            while (powerOf2 < capacity) {
                powerOf2 <<= 1;
            }
            m_capacity = powerOf2;
            m_mask = m_capacity - 1;
            m_buffer.resize(m_capacity);
        }
    }

    /**
     * @brief 入队操作
     * @param item 要添加的元素
     * @return bool 成功返回true，队列满返回false
     */
    bool enqueue(const T& item) {
        const size_t currentWrite = m_writeIndex.load(std::memory_order_relaxed);
        const size_t nextWrite = (currentWrite + 1) & m_mask;
        
        if (nextWrite == m_readIndex.load(std::memory_order_acquire)) {
            // 队列已满
            return false;
        }
        
        m_buffer[currentWrite] = item;
        m_writeIndex.store(nextWrite, std::memory_order_release);
        return true;
    }

    /**
     * @brief 强制入队（队列满时覆盖最旧元素）
     * @param item 要添加的元素
     */
    void forceEnqueue(const T& item) {
        const size_t currentWrite = m_writeIndex.load(std::memory_order_relaxed);
        const size_t nextWrite = (currentWrite + 1) & m_mask;
        
        if (nextWrite == m_readIndex.load(std::memory_order_acquire)) {
            // 队列已满，移动读指针丢弃最旧元素
            m_readIndex.store((m_readIndex.load(std::memory_order_relaxed) + 1) & m_mask, 
                             std::memory_order_release);
        }
        
        m_buffer[currentWrite] = item;
        m_writeIndex.store(nextWrite, std::memory_order_release);
    }

    /**
     * @brief 出队操作
     * @param item 用于接收出队元素的引用
     * @return bool 成功返回true，队列空返回false
     */
    bool dequeue(T& item) {
        const size_t currentRead = m_readIndex.load(std::memory_order_relaxed);
        
        if (currentRead == m_writeIndex.load(std::memory_order_acquire)) {
            // 队列为空
            return false;
        }
        
        item = m_buffer[currentRead];
        m_readIndex.store((currentRead + 1) & m_mask, std::memory_order_release);
        return true;
    }

    /**
     * @brief 检查队列是否为空
     * @return bool 空返回true
     */
    bool isEmpty() const {
        return m_readIndex.load(std::memory_order_acquire) == 
               m_writeIndex.load(std::memory_order_acquire);
    }

    /**
     * @brief 检查队列是否已满
     * @return bool 满返回true
     */
    bool isFull() const {
        const size_t nextWrite = (m_writeIndex.load(std::memory_order_acquire) + 1) & m_mask;
        return nextWrite == m_readIndex.load(std::memory_order_acquire);
    }

    /**
     * @brief 获取当前队列大小
     * @return size_t 当前元素数量
     */
    size_t size() const {
        const size_t write = m_writeIndex.load(std::memory_order_acquire);
        const size_t read = m_readIndex.load(std::memory_order_acquire);
        return (write - read) & m_mask;
    }

    /**
     * @brief 获取队列容量
     * @return size_t 队列容量
     */
    size_t capacity() const {
        return m_capacity;
    }

    /**
     * @brief 清空队列
     */
    void clear() {
        m_readIndex.store(m_writeIndex.load(std::memory_order_acquire), 
                         std::memory_order_release);
    }

    /**
     * @brief 获取队列使用率
     * @return double 使用率百分比 (0.0-1.0)
     */
    double getUsageRatio() const {
        return static_cast<double>(size()) / static_cast<double>(m_capacity);
    }

private:
    size_t m_capacity;                          ///< 队列容量
    size_t m_mask;                             ///< 容量掩码（用于快速取模）
    std::vector<T> m_buffer;                   ///< 环形缓冲区
    std::atomic<size_t> m_writeIndex;          ///< 写入索引
    std::atomic<size_t> m_readIndex;           ///< 读取索引
};