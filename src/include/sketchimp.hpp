#pragma once
#include <cstring>
#include <vector>
#include "defs.hpp"
#include "hash.hpp"

namespace SKETCH
{
    class BaseSketch
    {
    public:
        virtual void insert(data_t item) = 0;
        virtual count_t query(data_t item) {LOG_ERROR("TODO"); exit(-1);}
        virtual ~BaseSketch() = default;
    };

    class CMSketch : public BaseSketch
    {
    private:
    public:
        int nrows_;
        int len_;
        count_t** nt_;
        seed_t* seeds_;
    public:
        CMSketch(int nrows, int ncols) : nrows_(nrows), len_(ncols)
        {
            seeds_=new seed_t[nrows_];
            nt_=new count_t*[nrows_];
            for (int i=0;i<nrows_;i++)
            {
                seeds_[i]=clock();
                sleep(1);
                nt_[i]=new count_t[len_];
                memset(nt_[i], 0, sizeof(count_t)*len_);
            }
        }

        virtual ~CMSketch() override
        {
            delete[]seeds_;
            for (int i=0;i<nrows_;i++)
            {
                delete[]nt_[i];
            }
            delete[]nt_;
        }

        void insert(data_t item) override
        {
            for (int i=0;i<nrows_;i++)
            {
                int pos=HASH::hash(item, seeds_[i]) % len_;
                nt_[i][pos]++;
            }
        }

        count_t query(data_t item) override
        {
            count_t rst=INT32_MAX;
            for (int i=0;i<nrows_;i++)
            {
                int pos=HASH::hash(item, seeds_[i]) % len_;
                rst=std::min(rst, nt_[i][pos]);
            }
            return rst;
        }
    };

    class CountSketch : public BaseSketch
    {
    private:
        int nrows_;
        int len_;
        count_t** nt_;
        seed_t* seeds_;
        seed_t sseed;
    public:
        CountSketch(int nrows, int ncols) : nrows_(nrows), len_(ncols)
        {
            seeds_=new seed_t[nrows_];
            nt_=new count_t*[nrows_];
            for (int i=0;i<nrows_;i++)
            {
                seeds_[i]=clock();
                sleep(1);
                nt_[i]=new count_t[len_];
                memset(nt_[i], 0, sizeof(count_t)*len_);
            }
            sseed=clock();
        }

        void insert(data_t item) override
        {
            for (int i=0;i<nrows_;i++)
            {
                int pos=HASH::hash(item, seeds_[i]) % len_;
                nt_[i][pos] += HASH::hash(item, sseed);
            }
        }
    };

    class TowerSketch : public BaseSketch
    {
    private:
        int nrows_;
        int len_[4] = {};
        uint64_t threshold[4]={UINT32_MAX, UINT16_MAX, UINT8_MAX, 15};
        uint64_t* nt_[4];
        seed_t seeds_[4];
    public:
        TowerSketch(int nrows, int ncols)
        {
            if (nrows>4)
            {
                LOG_ERROR("@SKETCH::TowerSketch: to many rows!");
                exit(-1);
            }
            nrows_=nrows;
            len_[0]=ncols; len_[1]=2*ncols; len_[2]=4*ncols; len_[3]=8*ncols;
            for (int i=0;i<nrows;i++)
            {
                seeds_[i]=clock();
                sleep(1);
                nt_[i]=new uint64_t[len_[i]];
                memset(nt_[i], 0, 8*len_[i]);
            }
        }

        void insert(data_t item) override
        {
            for (int i=0;i<nrows_;i++)
            {
                int pos=HASH::hash(item, seeds_[i]) % len_[i];
                nt_[i][pos]++;
                if (nt_[i][pos]>threshold[i])
                    nt_[i][pos]=threshold[i];
            }
        }
    };
}