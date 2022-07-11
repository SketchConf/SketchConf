#include <chrono>
#include <cmath>
#include <fstream>
#include <random>
#include <algorithm>
#include <cstring>
#include <assert.h>
#include <unistd.h>
#include <iostream>
#include "logger.hpp"
#include "sketch.h"
#include "hash.hpp"
#include "util.h"
using namespace std;

std::vector<double> TowerSketch::ground_truth(count_t nrows, count_t ncols, const std::deque<Constraint>& constraints, StreamGen& stream)
{
    assert(nrows<=4);
    uint64_t** nt=new uint64_t*[nrows];
    for (int i=0;i<nrows;i++)
    {
        nt[i]=new uint64_t[8 * ncols];
        memset(nt[i], 0, ncols*64);
    }
    seed_t seeds[nrows];
    for (int i=0; i<nrows; i++)
    {
        seeds[i]=clock();
        sleep(1);
    }
    uint64_t threshold[4] = {UINT32_MAX, UINT16_MAX, UINT8_MAX, 15};
    int width[4]={ncols, 2*ncols, 4*ncols, 8*ncols};


    for (auto& i : stream.counter)
    {
        for (int j=0;j<nrows;j++)
        {
            int pos=HASH::hash(i.first, seeds[j]) % width[j];
            if (nt[j][pos]+i.second<threshold[j])
                nt[j][pos]=nt[j][pos]+i.second;
            else
                nt[j][pos]=threshold[j];
        }
    }

    vector<double> rst(constraints.size(), 0);
    for (auto& i : stream.counter)
    {
        uint64_t ans=UINT32_MAX;
        for (int j=0;j<nrows;j++)
        {
            int pos=HASH::hash(i.first, seeds[j]) % width[j];
            if (nt[j][pos]<threshold[j] && nt[j][pos]<ans)
                ans=nt[j][pos];
        }
        
        for (int j=0;j<constraints.size();j++)
        {
            if (ans-i.second > constraints[j].err)
                rst[j]++;
        }
    }
    
    int nflows=stream.getTotalFlows();
    for (auto& i : rst)
    {
        i=i/nflows;
    }
    return rst;
}

std::vector<double> TowerSketch::trivial_simulate(count_t nrows, count_t ncols, const std::deque<Constraint>& constraints, StreamGen& stream, int max_iter)
{
    if (nrows>4)
    {
        return vector<double>(constraints.size(), 1);
    }
    uint64_t threshold[4] = {UINT32_MAX, UINT16_MAX, UINT8_MAX, 15};
    int width[4]={ncols, 2*ncols, 4*ncols, 8*ncols};

    uint64_t** nt;
    nt=new uint64_t*[nrows];
    for (int i=0;i<nrows;i++)
    {
        nt[i]=new uint64_t[width[i]];
    }

    seed_t seeds[nrows];
    vector<double> rst(constraints.size(), 0);

    for (int u=0; u<max_iter; u++)
    {
        for (int i=0;i<nrows;i++)
        {
            memset(nt[i], 0, width[i]*sizeof(uint64_t));
        }
        for (int i=0; i<nrows; i++)
        {
            seeds[i]=clock();
            sleep(1);
        }
        vector<double> cur(constraints.size(), 0);
        bool stop_flag=true;

        count_t nflows=stream.getTotalFlows();
        uint32_t* flows=new uint32_t[nflows];
        for (count_t i=0;i<nflows;i++)
        {
            count_t freq=stream.new_stream();
            flows[i]=freq;
            for (int j=0;j<nrows;j++)
            {
                int pos=HASH::hash(i, seeds[j]) % width[j];
                if (nt[j][pos]+freq<threshold[j])
                    nt[j][pos]=nt[j][pos]+freq;
                else
                    nt[j][pos]=threshold[j];
            }
        }

        for (int i=0;i<nflows;i++)
        {
            uint64_t ans=UINT32_MAX;
            for (int j=0;j<nrows;j++)
            {
                int pos=HASH::hash(i, seeds[j]) % width[j];
                if (nt[j][pos]<threshold[j] && nt[j][pos]<ans)
                    ans=nt[j][pos];
            }
            
            for (int j=0;j<constraints.size();j++)
            {
                if (ans-flows[i] > constraints[j].err)
                    cur[j]++;
            }
        }

        for (int i=0;i<constraints.size();i++)
        {
            cur[i]=cur[i]/nflows;
            double tpcur=(cur[i]+rst[i]*u)/(u+1);
            if (fabs(tpcur-rst[i]) > STOP_THRESHOLD*rst[i])
            {
                stop_flag=false;
            }
            rst[i]=tpcur;
        }

        if (stop_flag)
            break;
    }
    
    return rst;
}


std::vector<double> TowerSketch::simulate_without_reuse(
        count_t nrows, count_t ncols, const std::deque<Constraint>& constraints,
        StreamGen& stream, bool trunc) 
{
    if (nrows>4)
    {
        return vector<double>(constraints.size(), 1);
    }
    static std::default_random_engine gen;
    double lambda = 1.0 / ncols;
    if (!trunc) 
    {
        vector<double> rst(constraints.size(), 0);
        vector<bool> stop_arr(constraints.size(), false);

        for (int u=0;;u++)
        {
            bool stop_flag=true;
            vector<double> cnt(constraints.size(), 0);
            for (int v=0;v<EPOCH;v++)
            {
                uint64_t cur=stream.new_stream();
                uint64_t threshold[4] = {UINT32_MAX, UINT16_MAX, UINT8_MAX, 15};
                int width[4]={ncols, 2*ncols, 4*ncols, 8*ncols};
                uint64_t counter[4]={};
                for (int i=0;i<4;i++)
                    counter[i]=min(cur, threshold[i]);

                for (int i=0;i<nrows;i++)
                {
                    double lambda = 1.0/width[i];
                    poisson_distribution<int> dist(stream.getTotalFlows() * lambda);
                    int num=dist(gen);

                    for (int k=0;k<num;k++)
                    {
                        uint64_t freq=stream.new_stream();
                        counter[i]=min(counter[i]+freq, threshold[i]);
                    }
                }
                uint64_t err=UINT32_MAX;
                for (int i=0;i<nrows;i++)
                {
                    if (counter[i]<threshold[i])
                    {
                        err=min(err, counter[i]-cur);
                    }
                }
                
                for (int i=0;i<constraints.size();i++)
                {
                    if (err>constraints[i].err)
                        cnt[i]++;
                }
            }

            for (int i=0;i<constraints.size();i++)
            {
                cnt[i]=cnt[i]/EPOCH;
                double tpcur=(cnt[i]+rst[i]*u)/(u+1);

                if (!stop(u+1, rst[i], tpcur, constraints[i].prob))
                    stop_flag=false;

                rst[i]=tpcur;
            }

            if (stop_flag)
                break;
        }
        return rst;
    } 
    else 
    {
        LOG_ERROR("TODO");
        return vector<double>(constraints.size(), 1);
    }
}

std::vector<double> TowerSketch::simulate_with_reuse(count_t nrows, count_t ncols, const std::deque<Constraint>& constraints, StreamGen& stream, bool trunc)
{
    if (nrows>4)
    {
        return vector<double>(constraints.size(), 1);
    }

    vector<double> width;
    vector<double> l;
    count_t nflows=stream.getTotalFlows();
    for (int i=0;i<nrows;i++)
    {
        width.push_back(ncols*(1<<i));
        l.push_back(nflows/width[i]);
    }
    arcinit(width,constraints,stream,trunc);
    vector<double> arcprob;
    for (int u=0;u<constraints.size();u++)
    {
        double cur=1;
        for (int i=0;i<nrows;i++)
        {
            int done=poisson_trunc(width[i], constraints[u], stream);
            double ppp=exp(-l[i]);
            double s=ppp*arctables[i][u][0];
            for (int k=1;k<=done;k++)
            {
                ppp=ppp*l[i]/k;
                s+=arctables[i][u][k]*ppp;
            }
            cur=cur*s;
        }
        arcprob.push_back(cur);
    }
    return arcprob;
}

void TowerSketch::arcinit(vector<double>& l, const std::deque<Constraint>& constraints, StreamGen& stream, bool trunc)
{
    uint64_t threshold[4] = {UINT32_MAX, UINT16_MAX, UINT8_MAX, 15};
    int nconstraints=constraints.size();

    for (int i=0;i<constraints.size();i++)
    {
        for (int u=0; u<l.size(); u++)
        {
            int done=poisson_trunc(l[u], constraints[i], stream);
            if (arctables[u][i].size() > done)
                continue;
            
            int let=arctables[u][i].size();
            if (!trunc)
            {
                for (int n=let; n<=done; n++)
                {
                    double arcnt=0;
                    for (int iii=0;;iii++)
                    {
                        double curcnt=0;

                        for (int v=0;v<EPOCH;v++)
                        {
                            uint64_t cur=stream.new_stream();
                            uint64_t err=min(cur, threshold[u]);
                            for (int j=0;j<n;j++)
                            {
                                err=err+stream.new_stream();
                                err=min(err, threshold[u]);
                            }
                            if (err==threshold[u])
                                err=UINT32_MAX;
                            if (err-cur > constraints[i].err)
                                curcnt++;
                        }

                        curcnt=curcnt/EPOCH;
                        curcnt=(curcnt+arcnt*iii)/(iii+1);
                        if (stop(iii+1, arcnt, curcnt))
                        {
                            arcnt=curcnt;
                            break;
                        }
                        else
                        {
                            arcnt=curcnt;
                        }
                    }
                    arctables[u][i].push_back(arcnt);
                }
            }
            else
            {
                LOG_ERROR("TODO");
                exit(-1);
            }
        }
    }
}


std::pair<double, double> TowerSketch::strawman_simulate(StreamGen& stream, const std::deque<Constraint>& constraints, count_t sum) 
{
    LOG_ERROR("BUGGY");
    double nflows=stream.TOTAL_PACKETS;
    int param[5]={0,8,32,64,64};
    int mem=INT32_MAX;
    int nrows=-1, ncols=-1;
    for (int i=1;i<=4;i++)
    {
        int l=-1;
        for  (auto& t : constraints)
        {
            double epsilon=t.err/nflows;
            double k=pow(param[i]*t.prob, 1.0/i) * epsilon;
            double cur=ceil(1.0/k);
            if (cur>l)
                l=cur;
        }
        if (i*4*l<mem)
        {
            mem=i*4*l;
            nrows=i;
            ncols=l;
        }
    }
    return make_pair(ncols, nrows);
}