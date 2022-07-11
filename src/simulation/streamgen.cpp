#include "include/defs.hpp"
#include "include/streamgen.h"
#include "include/logger.hpp"
#include "include/util.h"
#include <ctime>
#include <cstdlib>
#include <algorithm>
#include <cstring>
#include <string>
using namespace std;

data_t* StreamGen::read_data(const char *PATH, const int size)
{
    struct stat buf;
    LOG_DEBUG("Opening file %s", PATH);
    int fd=Open(PATH,O_RDONLY);
    fstat(fd,&buf);
    int n_elements = buf.st_size / size;
    cnt = n_elements;
    TOTAL_PACKETS=n_elements;
    LOG_DEBUG("\tcnt=%d", cnt);
    LOG_DEBUG("Mmap..."); 
    void* addr=mmap(NULL,buf.st_size,PROT_READ,MAP_PRIVATE,fd,0);
    data_t *data_array = new data_t[n_elements];
    close(fd);
    if (addr==MAP_FAILED)
    {
        LOG_ERROR("MMAP FAILED!");
        exit(-1);
    }

    char *ptr = reinterpret_cast<char *>(addr);
    for (int i = 0; i < n_elements; i++)
    {
        data_array[i] = *reinterpret_cast<data_t *>(ptr);
        ptr += size;
    }

    munmap(addr, buf.st_size);
    return data_array;
}

void StreamGen::init(string file, int size)
{
    raw_data = read_data(file.c_str(), size);
    for (count_t i = 0; i < cnt;i++)
    {
        auto it = counter.find(raw_data[i]);
        if (it==counter.end())
        {
            counter.insert(std::make_pair(raw_data[i], 1));
        }
        else
        {
            it->second++;
        }
    }
    TOTAL_FLOWS = counter.size();
    LOG_INFO("Total items: %d, Distinct items: %d", TOTAL_PACKETS, TOTAL_FLOWS);

    for (auto it : counter)
    {
        freqs.push_back(it.second);
    }

    srand(clock());
}

void StreamGen::init(vector<double>& dist)
{
    raw_data=NULL;
    TOTAL_PACKETS=0;
    TOTAL_FLOWS=0;
    for (int i=0;i<dist.size();i++)
    {
        if (dist[i]>0)
        {
            TOTAL_PACKETS += i*dist[i];
            TOTAL_FLOWS += dist[i];
            for (int k=0;k<dist[i];k++)
                freqs.push_back(i);
        }
    }
    LOG_INFO("Total flows: %d", TOTAL_FLOWS);
    LOG_INFO("Total packets: %d", TOTAL_PACKETS);
    cnt=TOTAL_PACKETS;
}

count_t StreamGen::new_stream()
{
    int pos=double(rand())*TOTAL_FLOWS/RAND_MAX;
    return freqs[pos];
}

void StreamGen::trunc_stream_init(count_t freq_lower_bound)
{
    for (int i=0;i<TOTAL_FLOWS;i++)
    {
        if (freqs[i] >= freq_lower_bound)
            freqs_big.push_back(freqs[i]);
        else
            freqs_small.push_back(freqs[i]);
    }
    TOTAL_BIG=freqs_big.size();
    TOTAL_SMALL=freqs_small.size();
}

count_t StreamGen::trunc_tiny_stream()
{
    double pos=double(rand())*TOTAL_SMALL/RAND_MAX;
    return freqs_small[pos];
}

count_t StreamGen::trunc_not_small_stream()
{
    double pos=double(rand())*TOTAL_BIG/RAND_MAX;
    return freqs_big[pos];
}
