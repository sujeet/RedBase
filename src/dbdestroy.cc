//
// dbcreate.cc
//
// Author: Jason McHugh (mchughj@cs.stanford.edu)
//
// This shell is provided for the student.

#include <iostream>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include "RM.h"
#include "SM.h"
#include "redbase.h"

using namespace std;

//
// main
//
int main(int argc, char *argv[])
{
    char *dbname;
    char command[255] = "rm -r ";

    // Look for 2 arguments. The first is always the name of the program
    // that was executed, and the second should be the name of the
    // database.
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " dbname \n";
        exit(1);
    }

    // The database name is the second argument
    dbname = argv[1];
    if (strlen (dbname) + strlen (command) >= sizeof (command)) {
        cerr << argv[0] << " database name too long: " << dbname << "\n";
        exit(1);
    }

    // Delete the DB subdirectory
    if (system (strcat(command,dbname)) != 0) {
        cerr << argv[0] << " cannot delete directory: " << dbname << "\n";
        exit(1);
    }

    return(0);
}
