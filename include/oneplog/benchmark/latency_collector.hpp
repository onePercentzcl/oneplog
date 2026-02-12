/**
 * @file latency_collector.hpp
 * @brief Latency sample collector for performance benchmarking
 * @brief 用于性能测试的延迟样本收集器
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

namespace oneplog::benchmark {

/**
 * @brief Latency sample collector with percentile calculation
 * @brief 支持百分位数计算的延迟样本收集器
 *
 * Collects latency samples and provides statistical analysis including
 * average, percentiles (P50, P99, P999), min, max, and standard deviation.
 *
 * 收集延迟样本并提供统计分析，包括平均值、百分位数（P50、P99、P999）、
 * 最小值、最大值和标准差。
 *
 * Requirements: 8.3, 8.4, 8.7
 */
class LatencyCollector {
public:
    /**
     * @brief Construct a latency collector with optional pre-allocation
     * @brief 构造延迟收集器，可选预分配
     * @param reserveSize Number of samples to pre-allocate / 预分配的样本数
     */
    explicit LatencyCollector(size_t reserveSize = 10000) {
        m_samples.reserve(reserveSize);
    }

    /**
     * @brief Add a latency sample
     * @brief 添加延迟样本
     * @param latencyNs Latency in nanoseconds / 延迟（纳秒）
     */
    void AddSample(double latencyNs) {
        m_samples.push_back(latencyNs);
        m_sorted = false;
    }

    /**
     * @brief Sort samples for percentile calculation
     * @brief 排序样本以计算百分位数
     *
     * Called automatically by GetPercentile() if needed.
     * 如果需要，GetPercentile() 会自动调用。
     */
    void Sort() {
        if (!m_sorted && !m_samples.empty()) {
            std::sort(m_samples.begin(), m_samples.end());
            m_sorted = true;
        }
    }

    /**
     * @brief Get percentile value
     * @brief 获取百分位数值
     * @param p Percentile (0.0 to 1.0, e.g., 0.99 for P99) / 百分位数（0.0 到 1.0）
     * @return Latency at the specified percentile / 指定百分位数的延迟
     */
    double GetPercentile(double p) {
        if (m_samples.empty()) {
            return 0.0;
        }
        
        // Ensure samples are sorted
        // 确保样本已排序
        Sort();
        
        // Calculate index using nearest-rank method
        // 使用最近秩方法计算索引
        size_t idx = static_cast<size_t>(std::ceil(p * static_cast<double>(m_samples.size()))) - 1;
        if (idx >= m_samples.size()) {
            idx = m_samples.size() - 1;
        }
        return m_samples[idx];
    }

    /**
     * @brief Get P50 (median) latency
     * @brief 获取 P50（中位数）延迟
     */
    double GetP50() { return GetPercentile(0.50); }

    /**
     * @brief Get P99 latency
     * @brief 获取 P99 延迟
     */
    double GetP99() { return GetPercentile(0.99); }

    /**
     * @brief Get P999 latency
     * @brief 获取 P999 延迟
     */
    double GetP999() { return GetPercentile(0.999); }

    /**
     * @brief Get average latency
     * @brief 获取平均延迟
     */
    double GetAverage() const {
        if (m_samples.empty()) {
            return 0.0;
        }
        double sum = std::accumulate(m_samples.begin(), m_samples.end(), 0.0);
        return sum / static_cast<double>(m_samples.size());
    }

    /**
     * @brief Get minimum latency
     * @brief 获取最小延迟
     */
    double GetMin() const {
        if (m_samples.empty()) {
            return 0.0;
        }
        return *std::min_element(m_samples.begin(), m_samples.end());
    }

    /**
     * @brief Get maximum latency
     * @brief 获取最大延迟
     */
    double GetMax() const {
        if (m_samples.empty()) {
            return 0.0;
        }
        return *std::max_element(m_samples.begin(), m_samples.end());
    }

    /**
     * @brief Get standard deviation
     * @brief 获取标准差
     */
    double GetStdDev() const {
        if (m_samples.size() < 2) {
            return 0.0;
        }
        
        double avg = GetAverage();
        double sumSquaredDiff = 0.0;
        for (double sample : m_samples) {
            double diff = sample - avg;
            sumSquaredDiff += diff * diff;
        }
        return std::sqrt(sumSquaredDiff / static_cast<double>(m_samples.size() - 1));
    }

    /**
     * @brief Get number of samples
     * @brief 获取样本数量
     */
    size_t Size() const { return m_samples.size(); }

    /**
     * @brief Check if collector is empty
     * @brief 检查收集器是否为空
     */
    bool Empty() const { return m_samples.empty(); }

    /**
     * @brief Clear all samples
     * @brief 清空所有样本
     */
    void Clear() {
        m_samples.clear();
        m_sorted = false;
    }

    /**
     * @brief Reserve capacity for samples
     * @brief 预留样本容量
     * @param capacity Number of samples to reserve / 要预留的样本数
     */
    void Reserve(size_t capacity) {
        m_samples.reserve(capacity);
    }

private:
    std::vector<double> m_samples;  ///< Latency samples / 延迟样本
    bool m_sorted{false};           ///< Whether samples are sorted / 样本是否已排序
};

}  // namespace oneplog::benchmark
