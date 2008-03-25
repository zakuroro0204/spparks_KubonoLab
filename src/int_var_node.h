/* ----------------------------------------------------------------------

------------------------------------------------------------------------- */

#ifndef INT_VAR_NODE_H
#define INT_VAR_NODE_H

#include <cmath>
#include "node.h"
#include <iostream>
#include "var_node.h"
namespace SPPARKS {
  class IntVarNode : public VarNode {
  public:
    IntVarNode();
    ~IntVarNode(){};
    inline double go(int index_in){return data_p[index_in];}
    inline double go(int index_in, int index_nb){
      //      std::cout <<"nb_flag = "<<nb_flag<<std::endl;
      return (nb_flag == 0)?(data_p[index_in]):(data_p[index_nb]) ;}
    
    void set_data_pointer(void *in);
    
    int *data_p;
  };
}



#endif