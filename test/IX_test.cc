#include "IX.h"
#include "pf.h"
#include "PF.h"
#include "ix.h"

#include <fstream>
#include <cstdio>

#include "gtest/gtest.h"

// Key count for integer keys     : 508
// Record count for a record page : 496

#define CLOSE() mgr.CloseIndex(handle)

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

#define MGR() PF_Manager _pfm; PF::Manager pfm(_pfm); IX::Manager mgr(pfm);
  

TEST (IX_Manager, init)
{
  MGR();
}

TEST (IX_Manager, CreateIndex)
{
  remove ("test");
  remove ("test.1");

  MGR();
  mgr.CreateIndex ("test", 1, INT, 4);
  IX::IndexHandle handle = mgr.OpenIndex ("test", 1);
  CLOSE ();

  EXPECT_TRUE (exists ("test.1"));
  remove ("test");
  remove ("test.1");
}

TEST (IX_Manager_wrapper, CloseHandleWithoutIndex)
{
  PF_Manager pfm;
  IX_Manager mgr (pfm);
  IX_IndexHandle handle;
  
  EXPECT_TRUE (mgr.CloseIndex (handle) != 0);
}

TEST (IX_Manager_wrapper, InsertIntoClosedIndex)
{
  remove ("test.1");

  PF_Manager pfm;
  IX_Manager mgr (pfm);
  IX_IndexHandle handle;
  
  mgr.CreateIndex ("test", 1, INT, 4);
  mgr.OpenIndex ("test", 1, handle);
  CLOSE ();

  int one = 1;
  RID rid (1, 1);
  EXPECT_TRUE (handle.InsertEntry ((void*)&one, rid) != 0);
  
  remove ("test.1");
}

TEST (IX_Manager_wrapper, DeleteFromClosedIndex)
{
  remove ("test.1");

  PF_Manager pfm;
  IX_Manager mgr (pfm);
  IX_IndexHandle handle;
  
  mgr.CreateIndex ("test", 1, INT, 4);
  mgr.OpenIndex ("test", 1, handle);

  int one = 1;
  RID rid (1, 1);
  handle.InsertEntry ((void*)&one, rid);
  CLOSE ();

  EXPECT_TRUE (handle.DeleteEntry ((void*)&one, rid) != 0);
  
  remove ("test.1");
}

TEST (IX_Manager, InsertIntoClosedIndex)
{
  remove ("test.1");

  MGR();

  mgr.CreateIndex ("test", 1, INT, 4);

  IX::IndexHandle handle = mgr.OpenIndex ("test", 1);
  CLOSE ();

  int one = 1;
  RID rid (1, 1);
  EXPECT_ANY_THROW (handle.Insert ((void*)&one, rid));
  
  remove ("test.1");
}

TEST (IX_Manager, DeleteFromClosedIndex)
{
  remove ("test.1");

  MGR();
  mgr.CreateIndex ("test", 1, INT, 4);

  IX::IndexHandle handle = mgr.OpenIndex ("test", 1);

  int one = 1;
  RID rid (1, 1);

  handle.Insert ((void*)&one, rid);
  CLOSE ();

  EXPECT_ANY_THROW (handle.Delete ((void*)&one, rid));
  
  remove ("test.1");
}

TEST (IX_Manager, CreateIndexBagArgs)
{
  remove ("test");
  MGR();

  EXPECT_THROW (mgr.CreateIndex ("test", -1, INT, 4),
                IX::error::BadArguments);

  remove ("test");
}

TEST (IX_Manager, InsertDuplicate)
{
  remove ("test.1");
  MGR();
  mgr.CreateIndex ("test", 1, INT, 4);
  IX::IndexHandle handle = mgr.OpenIndex ("test", 1);
  int one = 1;
  RID rid (1, 1);
  handle.Insert ((void*)&one, rid);

  EXPECT_THROW (handle.Insert ((void*)&one, rid),
                IX::error::DuplicateRID);

  CLOSE ();
  remove ("test.1");
}

TEST (IX_Manager, DeleteNonExistent)
{
  remove ("test.1");

  MGR();
  mgr.CreateIndex ("test", 1, INT, 4);
  IX::IndexHandle handle = mgr.OpenIndex ("test", 1);
  int one = 1;
  RID rid (1, 1);
  RID rid2 (2, 2);

  // Key doesn't exist
  EXPECT_THROW (handle.Delete ((void*)&one, rid),
                IX::error::RIDNoExist);

  // Key exists, but key rid pair doesn't
  handle.Insert ((void*)&one, rid);
  EXPECT_THROW (handle.Delete ((void*)&one, rid2),
                IX::error::RIDNoExist);

  // Double delete.
  handle.Delete ((void*)&one, rid);
  EXPECT_THROW (handle.Delete ((void*)&one, rid),
                IX::error::RIDNoExist);

  CLOSE ();
  remove ("test.1");
}

TEST (IX_Manager, SameKeyBucketChaining)
{
  remove ("test.1");

  MGR();
  mgr.CreateIndex ("test", 1, INT, 4);
  IX::IndexHandle handle = mgr.OpenIndex ("test", 1);

  // insert 2000 records, for the same key
  // (about 3 overflow buckets)
  int REC_COUNT = 5000;
  int key = 1;
  for (int i = 0; i < REC_COUNT; ++i) {
    RID rid (1, i);
    handle.Insert ((void*)&key, rid);
  }

  IX::Scan scan (handle, EQ_OP, (void*)&key);
  RID rid;
  int counter = 0;
  while ((rid = scan.next()) != IX::Scan::end) {
    EXPECT_EQ (rid.slot_num, counter++);
  }
  EXPECT_EQ (counter, REC_COUNT);

  CLOSE ();
  remove ("test.1");
}

void insert_seq_keys (int key_count)
{
  remove ("test.1");

  MGR();
  mgr.CreateIndex ("test", 1, INT, 4);
  IX::IndexHandle handle = mgr.OpenIndex ("test", 1);

  for (int key = 0; key < key_count; ++key) {
    RID rid (1, key);
    handle.Insert ((void*)&key, rid);
  }

  IX::Scan scan (handle, NO_OP, NULL);
  RID rid;
  int counter = 0;
  while ((rid = scan.next()) != IX::Scan::end) {
    EXPECT_EQ (rid.slot_num, counter++);
  }
  EXPECT_EQ (counter, key_count);

  CLOSE ();
  remove ("test.1");
}

TEST (IX_Manager, SingleLeafInsert)
{
  // One full page is 508 keys
  insert_seq_keys (508);

  // Fill only little
  insert_seq_keys (5);
}

TEST (IX_Manager, MultiLeafInsert)
{
  insert_seq_keys (3000);
}

// Insert a lot of RIDs under a single key.
// Delete them all.
TEST (IX_Manager, DeleteAllSingleKey)
{
  remove ("test.1");
  int key_count = 5000;

  MGR();
  mgr.CreateIndex ("test", 1, INT, 4);
  IX::IndexHandle handle = mgr.OpenIndex ("test", 1);

  int key = 1;
  // Insert records.
  for (int i = 0; i < key_count; ++i) {
    RID rid (1, i);
    handle.Insert ((void*)&key, rid);
  }

  // Delete those records.
  for (int i = 0; i < key_count; ++i) {
    RID rid (1, i);
    handle.Delete ((void*)&key, rid);
  }

  auto scan = new IX::Scan (handle, NO_OP, NULL);
  RID rid;
  int counter = 0;
  while ((rid = scan->next()) != IX::Scan::end) {
    counter++;
  }
  EXPECT_EQ (counter, 0);
  delete scan;

  CLOSE ();
  remove ("test.1");
}

// Insert a lot of RIDs under a single key.
// Delete them all by scanning.
TEST (IX_Manager, DeleteAllWhileScanningSingleKey)
{
  remove ("test.1");
  int key_count = 5000;

  MGR();
  mgr.CreateIndex ("test", 1, INT, 4);
  IX::IndexHandle handle = mgr.OpenIndex ("test", 1);

  int key = 1;
  // Insert records.
  for (int i = 0; i < key_count; ++i) {
    RID rid (1, i);
    handle.Insert ((void*)&key, rid);
  }

  // Delete those records.
  auto scan = new IX::Scan (handle, NO_OP, NULL);
  RID rid;
  while ((rid = scan->next()) != IX::Scan::end) {
    handle.Delete ((void*)&key, rid);
  }
  delete scan;

  scan = new IX::Scan (handle, NO_OP, NULL);
  int counter = 0;
  while ((rid = scan->next()) != IX::Scan::end) {
    counter++;
  }
  EXPECT_EQ (counter, 0);
  delete scan;

  CLOSE ();
  remove ("test.1");
}

// Insert a lot of RIDs under a single key.
// Delete all with odd slot number.
TEST (IX_Manager, DeleteHalfSingleKey)
{
  remove ("test.1");
  int key_count = 5000;

  MGR();
  mgr.CreateIndex ("test", 1, INT, 4);
  IX::IndexHandle handle = mgr.OpenIndex ("test", 1);

  int key = 1;
  // Insert records.
  for (int i = 0; i < key_count; ++i) {
    RID rid (1, i);
    handle.Insert ((void*)&key, rid);
  }

  // Delete the records with odd slot_num
  for (int i = 0; i < key_count; ++i) {
    RID rid (1, i);
    if (i % 2)
      handle.Delete ((void*)&key, rid);
  }

  auto scan = new IX::Scan (handle, NO_OP, NULL);
  RID rid;
  int counter = 0;
  while ((rid = scan->next()) != IX::Scan::end) {
    counter++;
    EXPECT_EQ (rid.slot_num % 2, 0);
  }
  EXPECT_EQ (counter, key_count / 2);
  delete scan;

  CLOSE ();
  remove ("test.1");
}

// Insert a lot of RIDs under a single key.
// Delete all with odd slot numbers while scanning.
TEST (IX_Manager, DeleteHalfWhileScanningSingleKey)
{
  remove ("test.1");
  int key_count = 5000;

  MGR();
  mgr.CreateIndex ("test", 1, INT, 4);
  IX::IndexHandle handle = mgr.OpenIndex ("test", 1);

  int key = 1;
  // Insert records.
  for (int i = 0; i < key_count; ++i) {
    RID rid (1, i);
    handle.Insert ((void*)&key, rid);
  }

  // Delete the records with odd slot_num
  auto scan = new IX::Scan (handle, NO_OP, NULL);
  RID rid;
  while ((rid = scan->next()) != IX::Scan::end) {
    if (rid.slot_num % 2)
      handle.Delete ((void*)&key, rid);
  }
  delete scan;

  scan = new IX::Scan (handle, NO_OP, NULL);
  int counter = 0;
  while ((rid = scan->next()) != IX::Scan::end) {
    counter++;
    EXPECT_EQ (rid.slot_num % 2, 0);
  }
  EXPECT_EQ (counter, key_count / 2);
  delete scan;

  CLOSE ();
  remove ("test.1");
}

TEST (IX_Manager, DeleteAllMultiKey)
{
  remove ("test.1");
  int key_count = 40;

  MGR();
  mgr.CreateIndex ("test", 1, INT, 4);
  IX::IndexHandle handle = mgr.OpenIndex ("test", 1);

  // Insert records.
  for (int key = 0; key < key_count; ++key) {
    RID rid (1, key);
    handle.Insert ((void*)&key, rid);
  }

  // Delete those records.
  for (int key = 0; key < key_count; ++key) {
    RID rid (1, key);
    handle.Delete ((void*)&key, rid);
  }

  auto scan = new IX::Scan (handle, NO_OP, NULL);
  RID rid;
  int counter = 0;
  while ((rid = scan->next()) != IX::Scan::end) {
    counter++;
  }
  EXPECT_EQ (counter, 0);
  delete scan;

  CLOSE ();
  remove ("test.1");
}

TEST (IX_Manager, StringInit)
{
  remove ("test.1");

  MGR();
  mgr.CreateIndex ("test", 1, STRING, 2);
  IX::IndexHandle handle = mgr.OpenIndex ("test", 1);
  RID rid (1, 1);
  char* hi = new char [2];
  hi[0] = 'h';
  hi[1] = 'i';
  handle.Insert (hi, rid);
  IX::Scan scan (handle, EQ_OP, hi);
  int counter = 0;
  while ((rid = scan.next()) != IX::Scan::end) {
    counter ++;
    EXPECT_EQ (rid.slot_num, 1);
  }
  EXPECT_EQ (counter, 1);

  delete[] hi;
  CLOSE ();
  remove ("test.1");
}

TEST (IX_Manager, LotOfStringKeys)
{
  int key_count = 5000;

  remove ("test.1");

  MGR();
  mgr.CreateIndex ("test", 1, STRING, 2);
  IX::IndexHandle handle = mgr.OpenIndex ("test", 1);
  
  char *key = new char [2];

  for (int i = 0; i < key_count; ++i) {
    RID rid (1, i);
    key [1] = i % 256;
    key [0] = i / 256;
    handle.Insert ((void*)&key, rid);
  }

  IX::Scan scan (handle, NO_OP, NULL);
  RID rid;
  int counter = 0;
  while ((rid = scan.next()) != IX::Scan::end) {
    EXPECT_EQ (rid.slot_num, counter++);
  }
  EXPECT_EQ (counter, key_count);

  delete[] key;
  CLOSE ();
  remove ("test.1");
}

TEST (IX_Manager, SingleStringKeyManyRecords)
{
  int key_count = 5000;

  remove ("test.1");

  MGR();
  mgr.CreateIndex ("test", 1, STRING, 2);
  IX::IndexHandle handle = mgr.OpenIndex ("test", 1);
  
  char *key = new char [2];
  key [0] = 'h';
  key [1] = 'i';

  for (int i = 0; i < key_count; ++i) {
    RID rid (1, i);
    handle.Insert ((void*)&key, rid);
  }

  key [0] = 'z';
  IX::Scan scan (handle, LE_OP, (void*)key);
  RID rid;
  int counter = 0;
  while ((rid = scan.next()) != IX::Scan::end) {
    EXPECT_EQ (rid.slot_num, counter++);
  }
  EXPECT_EQ (counter, key_count);

  delete[] key;
  CLOSE ();
  remove ("test.1");
}
