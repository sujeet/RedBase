//
// File:        parser_test.cc
// Description: Test the parser
// Authors:     Dallan Quass (quass@cs.stanford.edu)
//              Jason McHugh (mchughj@cs.stanford.edu)
//
// 1997: Changes for the statistics manager
//

#include <cstdio>
#include <iostream>
#include "redbase.h"
#include "parser.h"
#include "SM.h"
#include "ql.h"


//
// Global PF_Manager and RM_Manager variables
//
PF_Manager _pfm;
PF::Manager pfm (_pfm);
RM::Manager rmm (pfm);
IX::Manager ixm (pfm);
SM::Manager smm (ixm, rmm);
QL_Manager qlm (smm, ixm, rmm);

int main(void)
{
  try {
    smm.OpenDb ("testdb");
    RBparse(pfm, smm, qlm);
    smm.CloseDb ();
  }
  catch (exception& e) {
    cerr << e.what () << endl;
    return (1);
  }
}
