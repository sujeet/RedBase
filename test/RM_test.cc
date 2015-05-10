#include "RM.h"
#include "PF.h"

#include "rm.h"
#include "pf.h"

#include <fstream>
#include <cstdio>

#include "gtest/gtest.h"

#define CLOSE() mgr.CloseFile(handle)

inline bool exists (const std::string& name)
{
  ifstream f(name.c_str());
  if (f.good()) {
    f.close();
    return true;
  } else {
    f.close();
    return false;
  }   
}

#define MGR() PF_Manager _pfm; PF::Manager pfm(_pfm); RM::Manager mgr(pfm);

TEST (RM_Manager, init)
{
  MGR();
}

TEST (RM_Manager, CreateFile)
{
  remove ("test");
  MGR();
  mgr.CreateFile ("test", 4);
  RM::FileHandle handle = mgr.OpenFile ("test");
  CLOSE ();

  EXPECT_TRUE (exists ("test"));
  remove ("test");
}

TEST (RM_Manager, NullFileName)
{
  MGR();
  EXPECT_THROW (mgr.CreateFile (NULL, 4),
                RM::error::BadArgument);
}

TEST (RM_Manager, BadRecordSize)
{
  MGR();
  EXPECT_THROW (mgr.CreateFile ("whatever", 0),
                RM::error::BadArgument);
  EXPECT_THROW (mgr.CreateFile ("whatever", -8),
                RM::error::BadArgument);
}

TEST (RM_Manager, InsertVerifyClose)
{
  remove ("test");
  MGR();
  mgr.CreateFile ("test", 4);
  RM::FileHandle handle = mgr.OpenFile ("test");
  int NUM_RECS = 5000;
  bool seen [NUM_RECS];

  for (int i = 0; i < NUM_RECS; ++i) {
    seen [i] = false;
    handle.insert ((char*)&i);
  }
  RM::Scan scan;
  scan.open (handle, INT, 4, 0, LE_OP, &NUM_RECS);
  RM::Record r;
  int i;
  for (r = scan.next(), i = 0;
       r != scan.end;
       r = scan.next(), ++i) {
    seen [*(int*)r.data] = true;
  }
  EXPECT_EQ (i, NUM_RECS);

  for (int i = 0; i < NUM_RECS; ++i) {
    EXPECT_TRUE (seen [i]);
  }

  CLOSE ();
  scan.close ();

  EXPECT_TRUE (exists ("test"));
  remove ("test");
}
