#include "graph.h"
#include "error.h"
#include <cmath>
#include <set>
#include <vector>
#include <cstdlib>
#include <iostream>
#include <algorithm>
#include <fstream>
#include <string>
#include <sstream>
#include <map>
#include <limits>

//TODO no need to put only diagonal elements in Graph
RACE_error Graph::createGraphFromCRS(int *rowPtr, int *col, int *initPerm, int *initInvPerm)
{
    if(NROW != NCOL) {
        ERROR_PRINT("NROW!=NCOL : Currently Graph BMC supports only undirected Graph");
        return RACE_ERR_MATRIX_SYMM;
    }

    int nodeWithChildren = 0;

    std::vector<bool> pure_diag_flag(NROW,false);

    int nnz = 0;
#pragma omp parallel for schedule(runtime) reduction(+:nodeWithChildren) reduction(+:nnz)
    for(int row=0; row<NROW; ++row) {
        graphData[row].upperNnz=0;
        int permRow = row;
        if(initPerm)
        {
            permRow = initPerm[row];
        }
        int permCol = 0;
        permCol = col[rowPtr[permRow]];
        if(initInvPerm)
        {
            permCol = initInvPerm[permCol];
        }

        if(((rowPtr[permRow+1] - rowPtr[permRow]) == 1) && permCol == row)
        {
#pragma omp critical
            {
                pureDiag.push_back(row);
            }
        }
        //if( (rowPtr[permRow+1] - rowPtr[permRow]) > 1) {
            for(int idx=rowPtr[permRow]; idx<rowPtr[permRow+1]; ++idx) {
                permCol = col[idx];
                if(initInvPerm)
                {
                    permCol = initInvPerm[col[idx]];
                }
                graphData[row].children.push_back(permCol);
                //                if(permCol >= row)
                if(permCol >= permRow)
                {
                    ++graphData[row].upperNnz;
                }
                ++nnz;
            }
            nodeWithChildren++;
       /* }
        else
        {
#pragma omp critical
            {
                pureDiag.push_back(row);
            }
        }*/
    }

    NNZ = nnz;
    int irr_ctr=0;
    //resize Graph to the small size
    if(nodeWithChildren != NROW) {
        printf("ctr = %d\n", ++irr_ctr);
        ERROR_PRINT("Currently Graph has to be connected and irreducible");
        //return RACE_ERR_NOT_IMPLEMENTED;
        /*int ctr = 0;
        for(intIter iter=pureDiag.begin(); iter!=pureDiag.end(); ++iter) {
            graphData.erase(graphData.begin()+(*iter)-ctr);
            ++ctr;
        }*/
    }

    if(!pureDiag.empty())
    {
        ERROR_PRINT("Currently Graph has to be connected and irreducible");
    //    return RACE_ERR_NOT_IMPLEMENTED;
    }

    getStatistics();

    int ctr=0;
    int ctrSerial=0;
    perm.resize(NROW);
    //bring serial part to bottom and construct a graph out of rest
    for(int row=0; row<NROW; ++row)
    {
        if(std::find(serialPartRow.begin(), serialPartRow.end(), row)==serialPartRow.end())
        {
            perm[ctr] = row;
            ++ctr;
        }
        else
        {
            perm[(NROW) - NROW_serial + ctrSerial] = row;
            ++ctrSerial;
        }
    }

    //permute the graph
    permuteAndRemoveSerialPart();
    serialPart.resize(2);
    //Now delete the serialRows from graph
    NROW = NROW-NROW_serial;
    NNZ =  NNZ-NNZ_serial;
    serialPart[0] = NROW;
    serialPart[1] = NROW+NROW_serial;

    return RACE_SUCCESS;
}

//only for debugging puposes
void Graph::writePattern(char* name)
{
    std::string base_name(name);
    std::stringstream fileName;
    fileName<<base_name <<".mtx";
    std::fstream file;
    file.open( fileName.str().c_str() ,std::fstream::out);
    file<<"%";
    file<<"%MatrixMarket matrix coordinate real general\n";
    file<<"%MatrixMarketm file generated by RACE\n";

    file<<NROW<<" "<<NCOL<<" "<<NNZ<<"\n";

    for(int row=0; row<NROW; ++row) {
        for (unsigned j=0; j<graphData[row].children.size(); ++j) {
            file<<row+1<<" "<<graphData[row].children[j]+1<<" "<<10<<"\n";
        }
    }
}

void Graph::getStatistics()
{
    double maxDense = std::numeric_limits<double>::max();//default 8 times mean nnzr
    char *maxDenseEnv = getenv("RACE_MAX_DENSE");

    if(maxDenseEnv != NULL)
    {
        maxDense = atof(maxDenseEnv);
    }

    std::vector<int> blackList;
    std::map<int,int> rowBucket;
    int MEAN = NNZ/NROW;
    for(int row=0; row<NROW; ++row)
    {
        int rowLen = at(row).children.size();
        if(rowLen > maxDense*MEAN)
        {
            blackList.push_back(row);
        }
        rowBucket[rowLen] += 1;
    }
/*
    for(auto it=rowBucket.begin(); it!=rowBucket.end(); ++it)
    {
        printf("%d -> %d\n",it->first, it->second);
    }
*/
    //print black-listed rows
    if(!blackList.empty())
    {
        INFO_PRINT("Total of %lu rows separated due to high density", blackList.size());

        int nnzCtr = 0;
        //contribution of black-listed rows
        for(auto it = blackList.begin(); it!=blackList.end(); ++it)
        {
            int rowLen = at(*it).children.size();
            nnzCtr += rowLen;
        }

        NNZ_serial = nnzCtr;
        NROW_serial = blackList.size();
        INFO_PRINT("%5.2f %% is serialized", nnzCtr*100/((double)NNZ));
    }
    else
    {
        NNZ_serial = 0;
        NROW_serial = 0;
    }

    serialPartRow = blackList;

}


void Graph::permuteAndRemoveSerialPart()
{
    int *invPerm = new int[NROW];
    //create invPerm
    for(int i=0; i<NROW; ++i) {
        invPerm[perm[i]] = i;
    }

    Graph permutedGraph(*(this));

    //Permute rows
    for(int i=0; i<NROW; ++i)
    {
        permutedGraph.graphData[i].children = graphData[perm[i]].children;
    }

    //Permute columns
    for(int i=0; i<NROW; ++i)
    {
        std::vector<int> *children = &(permutedGraph.graphData[i].children);
        std::vector<int> newChildren;
        for(unsigned j=0; j<children->size(); ++j)
        {
            int child = children->at(j);
            int invChild = invPerm[child];

            if(invChild < (NROW-NROW_serial))
            {
                newChildren.push_back(invPerm[child]);
            }

            //remove children in serialPartRow
        }

        permutedGraph.graphData[i].children = newChildren;
    }

    permutedGraph.swap(*(this));
}

Graph::Graph(int nrow, int ncol, int *rowPtr, int *col, int *initPerm, int *initInvPerm):graphData(nrow),NROW(nrow),NCOL(ncol)
{
    RACE_FN(createGraphFromCRS(rowPtr, col, initPerm, initInvPerm));
}

Graph::Graph(const Graph& srcGraph):graphData(srcGraph.graphData),pureDiag(srcGraph.pureDiag),serialPartRow(srcGraph.serialPartRow),perm(srcGraph.perm),NROW(srcGraph.NROW),NCOL(srcGraph.NCOL), NROW_serial(srcGraph.NROW_serial), NNZ_serial(srcGraph.NNZ_serial)
{
}

RACE_error Graph::swap(Graph& other)
{
    if( (NROW != other.NROW) || (NCOL != other.NCOL)) {
        ERROR_PRINT("Incompatible Graphs");
        return RACE_ERR_INCOMPATIBILITY;
    }

    //swap graphData
    graphData.swap(other.graphData);
    pureDiag.swap(other.pureDiag);
    serialPartRow.swap(other.serialPartRow);
    perm.swap(other.perm);

    return RACE_SUCCESS;
}

Node& Graph::at(unsigned Idx)
{
    return graphData[Idx];
}

void Graph::getInitialPerm(int **perm_, int *len_)
{
    (*perm_) = new int[NROW+NROW_serial];

    for(int i=0; i<NROW+NROW_serial; ++i)
    {
        (*perm_)[i] = perm[i];
    }

    (*len_) = NROW + NROW_serial;
}
