#ifndef RACE_TRAVERSE_H
#define RACE_TRAVERSE_H
#include <vector>
#include <map>
#include "type.h"
#include "error.h"
#include "graph.h"
#include "levelData.h"
#include "macros.h"

class Graph;

class Traverse{
    private:
        Graph *graph;
        static std::map<int, LevelData> cachedData;
        RACE::dist dist;
        int rangeLo;
        int rangeHi;
        int parentIdx;
        int numRoots;
        //Size without pure diagonal elements
        int graphSize;

        int* distFromRoot;

        int* perm;
        int* invPerm;

        std::vector<std::vector<std::map<int, Range>>> boundaryRange;
        int totalNodesIncBoundary;

        //Level Details
        LevelData* levelData;
        std::vector<std::vector<std::map<int, LevelData*>>> boundaryLevelData;

        std::vector<int> markChildren(int currChild, int currLvl);
        RACE_error findLevelData(int lower_nrows, int upper_nrows, int totalLevel, LevelData* curLevelData);
        RACE_error createLevelData();
        void permuteGraph();
    public:
        //constructor
        Traverse(Graph *graph_, RACE::dist dist, int rangeLo_=0, int rangeHi_=-1, int parentIdx=0, int numRoots=1, std::vector<std::vector<std::map<int, Range>>> boundaryRange_={});
        ~Traverse();
        void calculateDistance();

        //deletion of array's after get calling get fns is user's responsibility
        void getPerm(int  **perm_, int *len);
        void getInvPerm(int **invPerm_, int *len);
        LevelData* getLevelData();
        std::vector<std::vector<std::map<int, LevelData*>>> getBoundaryLevelData();
};

struct Counter{
    static int val;
    static void add();
    static void reset();
};

#endif
