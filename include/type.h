#ifndef NAME_TYPE_H
#define NAME_TYPE_H
#include <vector>

enum dist_t{
    ONE=1,
    TWO=2
};

enum d2Method{
    TWO_BLOCK,
    THREE_BLOCK /*TODO: To be done*/
};

enum LB_t{
    ROW,
    NNZ
};

enum PinMethod{
    FILL,
    SCATTER
};

struct ZoneLeaf;

typedef std::vector<ZoneLeaf> tree_t;

#endif
