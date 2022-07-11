#pragma once
#include "defs.hpp"
#include <unordered_map>
#include <string>
#include <vector>

class StreamGen {
private:
    count_t cnt;
    std::vector<count_t> freqs;
    std::vector<count_t> freqs_small;
    std::vector<count_t> freqs_big;

    /**
     * @brief Read data from the dataset, store #package in cnt.
     *
     * @param PATH path of the dataset.
     * @param size size of each record.
     * @return array contains all the packages in the daatset, remember to
     * DELETE it after used!
     */
    data_t *read_data(const char *PATH, const int size);

public:
    count_t TOTAL_FLOWS;
    count_t TOTAL_PACKETS;
    count_t TOTAL_SMALL;
    count_t TOTAL_BIG;
    data_t* raw_data;
    std::unordered_map<data_t, count_t> counter;

    /**
     * @brief Should be called before calling other functions defined in this
     * file! Read the CAIDA dataset and figure out its distribution. Set
     * TOTAL_FLOWS and TOTAL_BUCKETS defined in datatype.h
     * 
     * @param file path of the data
     */
    void init(std::string file, int size);

    void init(std::vector<double>& dist);

    /**
     * @brief init() should be called first!
     * Generate a new stream w.r.t. the data distribution
     * @return # apperence of the generated stream
     */
    count_t new_stream();

    /**
     * @brief should be called before trunc_new_stream(). In order to sample
     * flow whose frequence is only in [min_freq, max_freq)
     *
     * @param freq_lower_bound lower bound of the frequency
     */
    void trunc_stream_init(count_t freq_lower_bound);

    /**
     * @brief generate stream whose frequency in [1, min_freq)
     *
     * @return count_t frequency of the generated stream
     */
    count_t trunc_tiny_stream();

    /**
     * @brief generate stream whose frequency in [min_freq, +infty)
     *
     * @return count_t frequency of the generated stream
     */
    count_t trunc_not_small_stream();

    auto getTotalFlows() const 
    {
        return this->TOTAL_FLOWS;
    }

    double L2_Norm()
    {
        double sum=0;
        for (auto& it : counter)
        {
            sum += it.second*it.second;
        }
        return sqrt(sum);
    }
};