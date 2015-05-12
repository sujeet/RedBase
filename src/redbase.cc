//
// redbase.cc
//
// Author: Jason McHugh (mchughj@cs.stanford.edu)
//
// This shell is provided for the student.

#include <iostream>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include "redbase.h"
#include "RM.h"
#include "SM.h"
#include "ql.h"

using namespace std;

PF_Manager _pfm;
PF::Manager pfm (_pfm);
RM::Manager rmm (pfm);
IX::Manager ixm (pfm);
SM::Manager smm (ixm, rmm);
QL_Manager qlm (smm, ixm, rmm);

int main(int argc, char *argv[])
{
  char *dbname;

  // Look for 2 arguments.  The first is always the name of the program
  // that was executed, and the second should be the name of the
  // database.
  if (argc != 2) {
    cerr << "Usage: " << argv[0] << " dbname \n";
    exit(1);
  }

  dbname = argv [1];
  try {
    smm.OpenDb (dbname);
    while (true) {
      try {
        RBparse (_pfm, smm, qlm);
        break;
      }
      catch (warning& w) {
        cerr << w.what () << endl;
        continue;
      }
      catch (exception& e) {
        throw;
      }
    }
    smm.CloseDb ();
  }
  catch (exception& e) {
    cerr << e.what () << endl;
    return (1);
  }

  cout << "Bye.\n";
}
