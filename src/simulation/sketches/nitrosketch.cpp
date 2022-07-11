#include <iostream>
#include <cstring>
#include <string>
#include <random>
#include <chrono>
#include <vector>
#include <deque>
#include <algorithm>
#include <unistd.h>
#include "logger.hpp"
#include "defs.hpp"
#include "sketch.h"
#include "hash.hpp"
#include "streamgen.h"
#include "util.h"
using namespace std;

static int C(int n, int k)
{
    uint64_t ans=1;
    for (int i=0;i<k;i++)
        ans*=(n-i);
    for (int i=1;i<=k;i++)
        ans/=i;
    return ans;
}

inline int trivial_hash()
{
    if (rand()%2)
        return 1;
    else
        return -1;
}

inline int trivial_hash(data_t item, seed_t seed)
{
    if (HASH::hash(item, seed) % 2)
        return 1;
    else
        return -1;
}

// ==============================================================================================================
// Nitro CM Sketch
// ==============================================================================================================

vector<double> NitroCMSketch::ground_truth(count_t nrows, count_t ncols, const std::deque<Constraint>& constraints, StreamGen& stream)
{
    seed_t* seeds = new seed_t[nrows];
    count_t** nt = new count_t*[nrows];
    for (int i=0;i<nrows;i++)
    {
        seeds[i]=clock();
        sleep(1);
        nt[i]=new count_t[ncols];
        memset(nt[i], 0, sizeof(count_t)*ncols);
    }

    int n=stream.TOTAL_PACKETS;
    srand(clock());
    for (int i=0;i<n;i++)
    {
        data_t cur=stream.raw_data[i];
        for (int k=0;k<nrows;k++)
        {
            if (double(rand())/RAND_MAX < sample_rate)
            {
                int pos = HASH::hash(cur, seeds[k]) % ncols;
                nt[k][pos]++;
            }
        }
    }

    vector<double> rst(constraints.size(), 0);
    for (const auto& it : stream.counter)
    {
        vector<count_t> tpcnt;
        for (int k=0;k<nrows;k++)
        {
            int pos = HASH::hash(it.first, seeds[k]) % ncols;
            tpcnt.push_back(nt[k][pos]);
        }
        sort(tpcnt.begin(), tpcnt.end());
        
        count_t curst;
        if (nrows % 2)
        {
            curst=tpcnt[(nrows-1)/2] / sample_rate;
        }
        else
        {
            curst=(tpcnt[nrows/2 - 1]+tpcnt[nrows/2]) / (2*sample_rate);
        }

        for (int i=0;i<constraints.size();i++)
        {
            if (abs(int(curst-it.second)) > constraints[i].err)
                rst[i]++;
        }
    }

    for (int i=0;i<constraints.size();i++)
        rst[i]=rst[i]/stream.getTotalFlows();
    return rst;
}

vector<double> NitroCMSketch::trivial_simulate(count_t nrows, count_t ncols, const std::deque<Constraint>& constraints, StreamGen& stream, int max_iter)
{
    uint64_t** nt;
    nt=new uint64_t*[nrows];
    for (int i=0;i<nrows;i++)
    {
        nt[i]=new uint64_t[ncols];
    }

    seed_t seeds[nrows];
    vector<double> rst(constraints.size(), 0);
    default_random_engine gen;

    for (int u=0; u<max_iter; u++)
    {
        for (int i=0;i<nrows;i++)
        {
            memset(nt[i], 0, ncols*sizeof(uint64_t));
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
            binomial_distribution<int> dist(freq, sample_rate);
            for (int j=0;j<nrows;j++)
            {
                int pos=HASH::hash(i, seeds[j]) % ncols;
                nt[j][pos]+=dist(gen);
            }
        }

        for (int i=0;i<nflows;i++)
        {
            vector<uint64_t> tpcnt;
            for (int k=0;k<nrows;k++)
            {
                int pos = HASH::hash(i, seeds[k]) % ncols;
                tpcnt.push_back(nt[k][pos]);
            }
            sort(tpcnt.begin(), tpcnt.end());
            
            uint64_t ans;
            if (nrows % 2)
            {
                ans=tpcnt[(nrows-1)/2] / sample_rate;
            }
            else
            {
                ans=(tpcnt[nrows/2 - 1]+tpcnt[nrows/2]) / (2*sample_rate);
            }
            
            for (int j=0;j<constraints.size();j++)
            {
                if (abs(int(ans-flows[i])) > constraints[j].err)
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

vector<double> NitroCMSketch::simulate_without_reuse(count_t nrows, count_t ncols, const std::deque<Constraint>& constraints, StreamGen& stream, bool trunc)
{
    if (nrows % 2 == 0)
    {
        LOG_DEBUG("Unsupported");
        return vector<double>(constraints.size(), 1);
    }
    
    if (!trunc)
    {
        vector<double> rst(constraints.size(), 0);
        default_random_engine gen;
        poisson_distribution<int> dist(double(stream.getTotalFlows())/ncols);
        vector<bool> stop_arr(constraints.size(), false);

        for (int u=0;;u++)
        {
            bool stop_flag=true;
            vector<double> curst(constraints.size(), 0);
            for (int i=0;i<EPOCH;i++)
            {
                count_t cur=stream.new_stream();
                count_t counter = binomial_distribution<int>(cur, sample_rate)(gen);
                
                int num=dist(gen);
                for (int k=0;k<num;k++)
                {
                    count_t tp=stream.new_stream();
                    counter += binomial_distribution<int>(tp, sample_rate)(gen);
                }

                counter = counter/sample_rate;
                int err=counter-cur;
                err=abs(err);
                for (int k=0;k<constraints.size();k++)
                {
                    if (err>constraints[k].err)
                        curst[k]++;
                }
            }

            for (int k=0;k<constraints.size();k++)
            {
                curst[k]=curst[k]/EPOCH;
                curst[k]=(curst[k]+rst[k]*u)/(u+1);
                if (!stop(u+1, rst[k], curst[k], constraints[k].prob))
                    stop_flag=false;

                rst[k]=curst[k];
            }

            if (stop_flag)
            {
                break;
            }
        }

        int st=(nrows+1)/2;
        for (int i=0;i<rst.size();i++)
        {
            double ans=0;
            for (int k=st; k<=nrows; k++)
            {
                ans+=C(nrows, k)*pow(rst[i], k)*pow(1-rst[i], nrows-k);
            }
            rst[i]=ans;
        }
        return rst;
    }
    else
    {
        LOG_ERROR("TODO");
        exit(-1);
    }
}

vector<double> NitroCMSketch::simulate_with_reuse(count_t nrows, count_t ncols, const std::deque<Constraint>& constraints, StreamGen& stream, bool trunc)
{
    if (nrows % 2 == 0)
    {
        LOG_DEBUG("Unsupported");
        return vector<double>(constraints.size(), 1);
    }

    double l;
    double done;
    count_t nflows=stream.getTotalFlows();
    l = double(nflows)/ncols;
    arcinit(ncols,constraints,stream,trunc);
    vector<double> arcprob;
    int st=(nrows+1)/2;
    for (int u=0;u<constraints.size();u++)
    {
        done = poisson_trunc(ncols, constraints[u], stream);
        double ppp=exp(-l);
        double s=ppp*arctables[u][0];
        for (int k=1;k<=done;k++)
        {
            ppp=ppp*l/k;
            s+=arctables[u][k]*ppp;
        }

        double ans=0;
        for (int k=st; k<=nrows; k++)
        {
            ans+=C(nrows, k)*pow(s, k)*pow(1-s, nrows-k);
        }
        arcprob.push_back(ans);
    }
    return arcprob;
}

void NitroCMSketch::arcinit(double l, const std::deque<Constraint>& constraints, StreamGen& stream, bool trunc)
{
    for (int i=0;i<arctables.size();i++)
    {
        int done=poisson_trunc(l, constraints[i], stream);
        if (arctables[i].size() > done)
            continue;
        
        int let=arctables[i].size();
        if (!trunc)
        {
            default_random_engine gen;
            for (int n=let; n<=done; n++)
            {
                double arcnt=0;
                for (int iii=0;;iii++)
                {
                    double curcnt=0;

                    for (int v=0;v<EPOCH;v++)
                    {
                        uint64_t cur=stream.new_stream();
                        uint64_t err=cur;
                        for (int j=0;j<n;j++)
                        {
                            err=err + stream.new_stream();
                        }
                        err = binomial_distribution<int>(err, sample_rate)(gen);
                        err=err/sample_rate;

                        if (abs(int(err-cur))>constraints[i].err)
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
                arctables[i].push_back(arcnt);
            }
        }
        else
        {
            LOG_ERROR("TODO");
            exit(-1);
        }
    }
}

pair<double,double> NitroCMSketch::strawman_simulate(StreamGen& stream, const deque<Constraint>& constraints,count_t sum)
{
    LOG_ERROR("TODO");
    exit(-1);
}




// ==============================================================================================================
// Nitro Count Sketch
// ==============================================================================================================
vector<double> NitroCountSketch::ground_truth(count_t nrows, count_t ncols, const std::deque<Constraint>& constraints, StreamGen& stream)
{
    seed_t* seeds = new seed_t[nrows];
    seed_t sseed;
    int** nt = new int*[nrows];
    for (int i=0;i<nrows;i++)
    {
        seeds[i]=clock();
        sleep(1);
        nt[i]=new int[ncols];
        memset(nt[i], 0, sizeof(int)*ncols);
    }
    sseed=clock();

    int n=stream.TOTAL_PACKETS;
    srand(clock());
    for (int i=0;i<n;i++)
    {
        data_t cur=stream.raw_data[i];
        for (int k=0;k<nrows;k++)
        {
            if (double(rand())/RAND_MAX < sample_rate)
            {
                int pos = HASH::hash(cur, seeds[k]) % ncols;
                nt[k][pos] += trivial_hash(cur, sseed);
            }
        }
    }

    vector<double> rst(constraints.size(), 0);
    for (const auto& it : stream.counter)
    {
        vector<int> tpcnt;
        for (int k=0;k<nrows;k++)
        {
            int pos = HASH::hash(it.first, seeds[k]) % ncols;
            tpcnt.push_back(nt[k][pos]);
        }
        sort(tpcnt.begin(), tpcnt.end());
        
        int curst;
        if (nrows % 2)
        {
            curst=tpcnt[(nrows-1)/2] / sample_rate;
        }
        else
        {
            curst=(tpcnt[nrows/2 - 1]+tpcnt[nrows/2]) / (2*sample_rate);
        }
        curst *= trivial_hash(it.first, sseed);

        for (int i=0;i<constraints.size();i++)
        {
            if (abs(int(curst-it.second)) > constraints[i].err)
                rst[i]++;
        }
    }

    for (int i=0;i<constraints.size();i++)
        rst[i]=rst[i]/stream.getTotalFlows();
    return rst;
}

vector<double> NitroCountSketch::trivial_simulate(count_t nrows, count_t ncols, const std::deque<Constraint>& constraints, StreamGen& stream, int max_iter)
{
    int** nt;
    nt=new int*[nrows];
    for (int i=0;i<nrows;i++)
    {
        nt[i]=new int[ncols];
    }

    seed_t seeds[nrows];
    seed_t sseed;
    vector<double> rst(constraints.size(), 0);
    default_random_engine gen;

    for (int u=0; u<max_iter; u++)
    {
        for (int i=0;i<nrows;i++)
        {
            memset(nt[i], 0, ncols*sizeof(int));
        }
        for (int i=0; i<nrows; i++)
        {
            seeds[i]=clock();
            sleep(1);
        }
        sseed=clock();
        vector<double> cur(constraints.size(), 0);
        bool stop_flag=true;

        count_t nflows=stream.getTotalFlows();
        int* flows=new int[nflows];
        for (count_t i=0;i<nflows;i++)
        {
            int freq=stream.new_stream();
            flows[i]=freq;
            binomial_distribution<int> dist(freq, sample_rate);
            for (int j=0;j<nrows;j++)
            {
                int pos=HASH::hash(i, seeds[j]) % ncols;
                nt[j][pos] += dist(gen)*trivial_hash(i, sseed);
            }
        }

        for (int i=0;i<nflows;i++)
        {
            vector<int> tpcnt;
            for (int k=0;k<nrows;k++)
            {
                int pos = HASH::hash(i, seeds[k]) % ncols;
                tpcnt.push_back(nt[k][pos]);
            }
            sort(tpcnt.begin(), tpcnt.end());
            
            int ans;
            if (nrows % 2)
            {
                ans=tpcnt[(nrows-1)/2] / sample_rate;
            }
            else
            {
                ans=(tpcnt[nrows/2 - 1]+tpcnt[nrows/2]) / (2*sample_rate);
            }
            ans *= trivial_hash(i, sseed);
            
            for (int j=0;j<constraints.size();j++)
            {
                if (abs(int(ans-flows[i])) > constraints[j].err)
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

vector<double> NitroCountSketch::simulate_without_reuse(count_t nrows, count_t ncols, const std::deque<Constraint>& constraints, StreamGen& stream, bool trunc)
{
    if (nrows % 2 == 0)
    {
        LOG_DEBUG("Unsupported");
        return vector<double>(constraints.size(), 1);
    }
    
    if (!trunc)
    {
        vector<double> rst(constraints.size(), 0);
        default_random_engine gen;
        binomial_distribution<int> dist(stream.getTotalFlows(), 1.0/ncols);
        vector<bool> stop_arr(constraints.size(), false);

        for (int u=0;;u++)
        {
            bool stop_flag=true;
            vector<double> curst(constraints.size(), 0);
            for (int i=0;i<EPOCH;i++)
            {
                int cur=stream.new_stream();
                int counter = binomial_distribution<int>(cur, sample_rate)(gen);
                
                int num=dist(gen);
                for (int k=0;k<num;k++)
                {
                    int tp=stream.new_stream();
                    counter += binomial_distribution<int>(tp, sample_rate)(gen) * trivial_hash();
                }

                counter = counter/sample_rate;
                int err=counter-cur;
                for (int k=0;k<constraints.size();k++)
                {
                    if (err>constraints[k].err)
                        curst[k]++;
                }
            }

            for (int k=0;k<constraints.size();k++)
            {
                curst[k]=curst[k]/EPOCH;
                curst[k]=(curst[k]+rst[k]*u)/(u+1);
                if (!stop(u+1, rst[k], curst[k], constraints[k].prob))
                    stop_flag=false;

                rst[k]=curst[k];
            }

            if (stop_flag)
            {
                break;
            }
        }

        int st=(nrows+1)/2;
        for (int i=0;i<rst.size();i++)
        {
            double ans=0;
            for (int k=st; k<=nrows; k++)
            {
                ans+=C(nrows, k)*pow(rst[i], k)*pow(1-rst[i], nrows-k);
            }
            rst[i]=ans*2;
        }
        return rst;
    }
    else
    {
        LOG_ERROR("TODO");
        exit(-1);
    }
}

vector<double> NitroCountSketch::simulate_with_reuse(count_t nrows, count_t ncols, const std::deque<Constraint>& constraints, StreamGen& stream, bool trunc)
{
    if (nrows % 2 == 0)
    {
        LOG_DEBUG("Unsupported");
        return vector<double>(constraints.size(), 1);
    }

    double l;
    double done;
    int nflows=stream.getTotalFlows();
    l = double(nflows)/ncols;
    arcinit(ncols,constraints,stream,trunc);
    vector<double> arcprob;
    int st=(nrows+1)/2;
    for (int u=0;u<constraints.size();u++)
    {
        done=poisson_trunc(ncols,constraints[u],stream);
        double ppp=exp(-l);
        double s=ppp*arctables[u][0];
        for (int k=1;k<=done;k++)
        {
            ppp=ppp*l/k;
            s+=arctables[u][k]*ppp;
        }

        double ans=0;
        for (int k=st; k<=nrows; k++)
        {
            ans+=C(nrows, k)*pow(s, k)*pow(1-s, nrows-k);
        }
        arcprob.push_back(ans*2);
    }
    return arcprob;
}

void NitroCountSketch::arcinit(double l, const std::deque<Constraint>& constraints, StreamGen& stream, bool trunc)
{
    for (int i=0;i<arctables.size();i++)
    {
        int done=poisson_trunc(l, constraints[i], stream);
        if (arctables[i].size() > done)
            continue;
        
        int let=arctables[i].size();
        if (!trunc)
        {
            default_random_engine gen;
            for (int n=let; n<=done; n++)
            {
                double arcnt=0;
                for (int iii=0;;iii++)
                {
                    double curcnt=0;

                    for (int v=0;v<EPOCH;v++)
                    {
                        int cur=stream.new_stream();
                        int err=binomial_distribution<int>(cur, sample_rate)(gen);
                        int tpcnt1=0, tpcnt2=0;
                        for (int j=0;j<n;j++)
                        {
                            int tps=stream.new_stream();
                            if (trivial_hash()==1)
                                tpcnt1 += tps;
                            else
                                tpcnt2 += tps;
                        }
                        tpcnt1=binomial_distribution<int>(tpcnt1, sample_rate)(gen);
                        tpcnt2=binomial_distribution<int>(tpcnt2, sample_rate)(gen);
                        err=err+tpcnt1-tpcnt2;
                        err=err/sample_rate;

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
                arctables[i].push_back(arcnt);
            }
        }
        else
        {
            LOG_ERROR("TODO");
            exit(-1);
        }
    }
}

pair<double,double> NitroCountSketch::strawman_simulate(StreamGen& stream, const deque<Constraint>& constraints,count_t sum)
{
    LOG_ERROR("TODO");
    exit(-1);
}
