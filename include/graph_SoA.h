#ifndef RACE_GRAPH_SOA_H
#define RACE_GRAPH_SOA_H

#include "print.h"
#include "error.h"
#include <vector>


class Graph{
    private:
       /**
         * @brief Graph of the matrix.
         */
        int* graphData;
        int* tmpGraphData;
        int* childrenStart;
        int* childrenSize;
        /**
         * @brief Diagonal Elements of the matrix (may be removed in future).
         */
        std::vector<int> pureDiag;
        /**
         * @brief Serially procedssed row list
         */
        std::vector<int> serialPartRow;
        /**
         * @brief Create Graph from sparsematrix in CRS fromat.
         *
         * @param[in] rowPtr rowPtr of the matrix.
         * @param[in] col column index of the matrix.
         */
        RACE_error createGraphFromCRS(int *rowPtr, int *col, int *initPerm=NULL, int *initInvPerm=NULL);

        std::vector<int> serialPerm;
        void permuteAndRemoveSerialPart();
    public:
        /*Store total permutation vectors*/
        int* totalPerm;
        int* totalInvPerm;

       /**
         * @brief Number of Rows in the matrix.
         */
        int NROW;
        /**
         * @brief Number of Columns in the matrix.
         */
        int NCOL;
        int NROW_serial;
        int NNZ;
        int NNZ_serial;
        std::vector<int> serialPart;
        Graph(int nrow, int ncol, int *row_ptr, int *col, int *initPerm=NULL, int *initInvPerm=NULL);//constructor
        Graph(const Graph &srcGraph);//copy constructor
        ~Graph();
        void writePattern(char *name);
        //returns a list of nodes having load more than RACE_MAX_LOAD
        bool getStatistics();
        //returns only permutation for serial part
        void getSerialPerm(int **perm_, int *len_);
        //if used control will be passed to calling class
        void getPerm(int **perm_, int *len);
        void getInvPerm(int **invPerm_, int *len);
        int getChildrenSize(int row);
        int* getChildren(int row);

        RACE_error swap(Graph& other);
        /**
         * @brief Traverse class is responsible for traversing through graph
         * and building levelPtr
         */
        friend class Traverse;
};
#endif
