#include "PF.h"
#include "pf.h"
#include "pf_internal.h"
#include "pf_hashtable.h"

#include <cstdio>
#include <fstream>

#include "gtest/gtest.h"

using namespace std;

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

#define MK_MGR() PF_Manager _pfm; PF::Manager mgr (_pfm);
  
TEST (PF_Manager, init)
{
  PF_Manager _pfm;
  PF::Manager pfm (_pfm);
}

TEST (PF_Manager, CreateFile)
{
  remove ("test_file");
  remove ("another_test");

  MK_MGR ();
  mgr.CreateFile ("test_file");

  EXPECT_TRUE (exists ("test_file"));
  EXPECT_FALSE (exists ("another_test"));

  remove ("test_file");
  remove ("another_test");
}

TEST (PF_Manager, OpenNonExistentFile)
{
  MK_MGR ();
  try {
    PF::FileHandle handle = mgr.OpenFile ("nonexistent");
    EXPECT_TRUE (4 == 5);
  }
  catch (PF::error::SystemError& e) {
    EXPECT_TRUE (true);
  }
  catch (exception& e) {
    EXPECT_TRUE (99 == 999);
  }
}

TEST (PF_Manager, OpenExisting)
{
  remove ("test_file");
  MK_MGR ();
  mgr.CreateFile ("test_file");
  PF::FileHandle handle = mgr.OpenFile ("test_file");
  remove ("test_file");
}

TEST (PF_Manager, AddPageToFile)
{
  remove ("test_file");
  MK_MGR ();
  mgr.CreateFile ("test_file");
  PF::FileHandle handle = mgr.OpenFile ("test_file");
  PF::PageHandle page = handle.AllocatePage ();
  handle.MarkDirty (page.GetPageNum());
  char *data = page.GetData ();
  data [0] = 'a';
  handle.UnpinPage (page);
  mgr.CloseFile (handle);
  remove ("test_file");
}
