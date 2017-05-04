#ifndef NAME_MACHINE_H
#define NAME_MACHINE_H

#include <hwloc.h>
#include "error.h"
#include <vector>

struct PUNode{
    hwloc_cpuset_t puSet;
    int rank;
};

inline bool operator<(const PUNode &a, const PUNode &b)
{
    return (a.rank < b.rank);
}

class Machine{
    private:
        hwloc_topology_t topology;
        std::vector<std::vector<PUNode>> topTree;
        std::vector<int> corePerNode;

        bool SMT;
        int numNode;
        int numCore;
        int numPU;

        void initTopology();
        void sortSMT();
    public:
        //constructor
        Machine(bool SMT_);
        //Machine();
        ~Machine();
        int getNumNode();
        int getNumPU();
        int getNumCore();
        int getNumPuInNode(int logicalNodeid);
        int getNumCoreInNode(int logicalNodeid);
        NAME_error pinThread(int logicalPUid, int logicalNodeId);
        void printTopTree();
};

#endif
