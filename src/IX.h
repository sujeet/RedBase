#ifndef __IX_H
#define __IX_H

#include "PF.h"
#include "rm_rid.h"
#include "Array.h"
#include "bitmap.h"

namespace IX
{
class Manager;
class Scan;

class IndexHandle {
  friend class Manager;
  friend class Scan;

private:
  PF::FileHandle index_file;
  bool uninitialized;

  IndexHandle (PF::FileHandle& index_file);
  PF::PageHandle GetRoot () const;
  PF::PageHandle GetFirstLeaf () const;

public:
  IndexHandle (): uninitialized (true) {}
  IndexHandle (const IndexHandle& other);
  void Insert (const void *data, const RID &rid);
  void Delete (const void *data, const RID &rid);
  void ForcePages () const;
};

class Scan
{
private:
  CompOp compOp;
  const char* dummy = "dummydummy\0";
  PF::FileHandle index_file;
  ArrayElem key;
  PageNum leaf_page_num;
  PageNum bucket_page_num;
  int rid_i;
  int key_i;

  void next_rid_i ();
  void next_bucket_page_num ();
  void next_key_i ();
  void next_leaf_page_num ();
  bool satisfy (const ArrayElem& key);
  
public:
  static const RID end;

  Scan (const IndexHandle &indexHandle,
        CompOp compOp,
        void *value);
  RID next ();
};

class Manager
{
private:
  PF::Manager pfm;

public:
  Manager (PF::Manager &pfm);
  Manager (PF_Manager &pfm);
  void CreateIndex (const char* fileName,
                    int indexNo,
                    AttrType attrType,
                    int attrLength);
  void DestroyIndex (const char *fileName, int indexNo);
  IndexHandle OpenIndex (const char *fileName, int indexNo);
  void CloseIndex (IndexHandle &indexHandle);
};

class KeyAndPageNum
{
public:
  ArrayElem key;
  PageNum page_num;

  KeyAndPageNum (AttrType type, int len, char* data, PageNum page_num);
  KeyAndPageNum (const ArrayElem& key, PageNum page_num);
  KeyAndPageNum ();

  // so that we can write the following code:
  // KeyAndPageNum ret = func ();
  // if (ret == NULL) do_something ();
  bool operator == (void* null);
  bool operator != (void* null);
};

struct TreePageHdr
{
  int num_keys;
  int key_size;
  AttrType key_type;
  bool is_root;
  bool is_leaf;
};

class TreePage
{
  friend class IndexHandle;
  friend class Scan;
private:
  int max_num_keys;
  TreePageHdr* hdr;
  Array keys;
  Array page_nums;

  bool is_full () const;
  ArrayElem pop_key ();
  void add (int key_idx,
            const ArrayElem& key,
            int page_num_idx,
            PageNum page_num);

  // common code for both constructors.
  void init (char* start_of_page);

  // ONLY FOR PAGES THAT ARE FULL
  //
  // Consider a max_key_count=4 page:
  //             * this page *
  // keys:      k1  k2  k3  k4
  // pages:   p1  p2  p3  p4  p5
  //
  // transfer 2 keys:
  //  * this page *  |   * new page *
  //     k1  k2      |      k3  k4
  //   p1  p2  p3    |    p3  p4  p5
  // 
  // transfer 1 keys:
  //    * this page *  | * new page *
  //     k1  k2  k3    |      k4
  //   p1  p2  p3  p4  |    p4  p5
  void transferkeys (PF::PageHandle& sibling_page,
                     int num_keys_to_transfer);
  void Delete (const ArrayElem& key,
               const RID& rid,
               PF::FileHandle& index_file);

public:
  TreePage (PF::PageHandle& pf_page);
  TreePage (char* start_of_page);
  TreePage (char* start_of_page, const TreePageHdr& hdr);

  // Insert the key and RID in the tree rooted at this page
  // If this node split, return the page number of the newly created
  // sibling, as well as the key that should be inserted into the parent.
  KeyAndPageNum insert (const ArrayElem& key,
                        const RID& rid,
                        PF::FileHandle& index_file);

};

struct RIDPageHdr
{
  PageNum next_page;
  int rid_count;
};

class RIDPage
{
  friend class Scan;
  friend class TreePage;

private:
  RIDPageHdr* hdr;
  Bitmap bitmap;
  RID* rids;
  int max_rid_count;

  void clear ();
  bool is_full () const;
  void add (const RID& rid);

public:
  RIDPage (PF::PageHandle& page_handle);
  void insert (const RID& rid, PF::FileHandle& index_file);
  void Delete (const RID& rid, PF::FileHandle& index_file);
};

#define DECLARE_EXCEPTION(name, message)   \
class name: public exception               \
{                                          \
  virtual const char* what() const throw() \
  {                                        \
    return message;                        \
  }                                        \
};

namespace error
{
DECLARE_EXCEPTION (BadArguments, "Bad arguments.");
DECLARE_EXCEPTION (DuplicateRID, "Tried to insert a duplicate entry.");
DECLARE_EXCEPTION (RIDNoExist, "Tried to delete a non-existent entry.");
DECLARE_EXCEPTION (UninitializedIndexHandle,
                   "Tried to do operations on uninitialized IndexHandle");
}      // namespace error

}      // namespace IX
#endif  // __IX_H
