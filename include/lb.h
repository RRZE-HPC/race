#ifndef NAME_LB_H
#define NAME_LB_H

#include "type.h"
#include "levelData.h"
#include "error.h"

enum neighbour_t{
    acquire,
    give
};

class Stat{
    private:
        int *arr;
        int len;

    public:
        Stat(int *arr_, int len_, int numPartitions_);
        ~Stat();
        int numPartitions;
        void calculate();
        double *mean;
        double *var;
        double *acquireWeight;
        double *giveWeight;
        double *weight;
};

class LB{
    private:
        int* levelPtr;
        int* zonePtr;
        LevelData* levelData;

        int maxThreads;
        int nThreads;
        int blockPerThread;
        int nBlocks;

        d2Method d2Type;
        LB_t lbTarget;

        void calcChunkSum(int *arr, bool forceRow=false);
        void calcZonePtr(int base);
        int  findNeighbour(const Stat &stats, neighbour_t type, dist_t x);
        void moveOneStep(int toIdx, int fromIdx);
        void splitZones(dist_t dist);

    public:
        LB(int nThreads_, LevelData* levelData_, LB_t lbTarget = NNZ); //constructor
        ~LB(); //destructor

        int getMaxThreads();
        int getNumThreads();
        void getZonePtr(int **zonePtr_, int *len, int base=0);
        NAME_error D2LB();
        NAME_error D1LB();
};


#endif
