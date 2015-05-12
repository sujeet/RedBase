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
    char command[255] = "mkdir ";

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

    // Create a subdirectory for the database
    if (system (strcat(command,dbname)) != 0) {
        cerr << argv[0] << " cannot create directory: " << dbname << "\n";
        exit(1);
    }

    if (chdir(dbname) < 0) {
        cerr << argv[0] << " chdir error to " << dbname << "\n";
        exit(1);
    }

    // Create the system catalogs...
    PF_Manager _pfm;
    PF::Manager pfm (_pfm);
    RM::Manager rmm (pfm);
    try {
      rmm.CreateFile ("relcat", sizeof (Table));
      rmm.CreateFile ("attrcat", sizeof (Attribute));

      // Store info about relcat and attrcat tables
      auto relcat = rmm.OpenFile ("relcat");
      Table relcat_table ("relcat", sizeof (Table), 5, 0, 1);
      Table attrcat_table ("attrcat", sizeof (Attribute), 6, 0, 1);
      relcat.insert ((const char*)&relcat_table);
      relcat.insert ((const char*)&attrcat_table);
      rmm.CloseFile (relcat);
      
      // Store info about all the attributes of relcat and attrcat tables
      auto attrcat = rmm.OpenFile ("attrcat");
      int sl = MAXNAME + 1;

      // relcat attributes
      const char* r = "relcat";
      Attribute t_name           (r, "relName",        0,  STRING, sl, -1);
      Attribute t_row_len        (r, "tupleLength",    sl,    INT,  4, -1);
      Attribute t_attr_count     (r, "attrCount",      sl+4,  INT,  4, -1);
      Attribute t_index_count    (r, "index_count",    sl+8,  INT,  4, -1);
      Attribute t_next_index_num (r, "next_index_num", sl+12, INT,  4, -1);
      attrcat.insert ((const char*)&t_name);
      attrcat.insert ((const char*)&t_row_len);
      attrcat.insert ((const char*)&t_attr_count);
      attrcat.insert ((const char*)&t_index_count);
      attrcat.insert ((const char*)&t_next_index_num);

      // attrcat attributes
      const char* a = "attrcat";
      Attribute a_table_name (a, "relName",    0,      STRING, sl, -1);
      Attribute a_name       (a, "attrName",   sl,     STRING, sl, -1);
      Attribute a_offset     (a, "offset",     2*sl,      INT,  4, -1);
      Attribute a_type       (a, "attrType",   2*sl + 4,  INT,  4, -1);
      Attribute a_len        (a, "attrLength", 2*sl + 8,  INT,  4, -1);
      Attribute a_index_num  (a, "indexNo",    2*sl + 12, INT,  4, -1);
      attrcat.insert ((const char*)&a_table_name);
      attrcat.insert ((const char*)&a_name);
      attrcat.insert ((const char*)&a_offset);
      attrcat.insert ((const char*)&a_type);
      attrcat.insert ((const char*)&a_len);
      attrcat.insert ((const char*)&a_index_num);
      rmm.CloseFile (attrcat);
      
    }
    catch (exception& e) {
      cerr << argv[0] << ": " << e.what () << endl;
      exit(1);
    }

    return(0);
}
