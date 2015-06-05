#ifndef __RM_H
#define __RM_H

#include "PF.h"
#include "rm_rid.h"
#include "Array.h"
#include "bitmap.h"
#include "Blob.h"

namespace RM
{
class Manager;
class FileHandle;
class Scan;

struct PageHdr
{
  int num_records;
  PageNum next_page;

  /* This field is relevant only for header pages */
  int next_blob_id_available;
};


class Record
{
private:
  char data_ [PF::kPageSize];

public:
  RID rid;
  char* data;

  Record ();
  Record (const Record& other);
  Record& operator = (const Record& other);

  bool operator == (const Record& other) const;
  bool operator != (const Record& other) const;
};


class Page
{
  friend class FileHandle;
  friend class Scan;
  friend class Manager;

private:
  PF::PageHandle pf_page;
  int record_size;
  int max_num_records;
  PageHdr* hdr;
  Bitmap bitmap;
  char* records;
  Page () {}

public:
  Page (PF::PageHandle& pf_page, int record_size);

  void clear ();

  Record get (SlotNum slot_num) const;
  SlotNum insert (const char* rec_data);
  void Delete (SlotNum slot_num);
  void update (const Record& rec);

  PageNum GetPageNum () const;
  void SetNextPageNum (PageNum next_page_num);
  bool full () const;
};


class HeaderPage
{
private:
  PF::PageHandle pf_page;

public:
  HeaderPage (PF::PageHandle& pf_page);

  void clear ();

  int GetRecordSize () const;
  void SetRecordSize (int record_size);
  int GetNextAvailableBlobId () const;

  PageNum GetFirstPageNum () const;
  void SetFirstPageNum (PageNum first_page_num);
  void SetNextAvailableBlobId (int next_available_blob_id);
};


class FileHandle
{
  friend class Manager;
  friend class Scan;
  friend class RelIterator;

private:
  PF::FileHandle pf_file_handle;
public:
  int record_size;
private:
  bool header_modified;
  PageNum first_page_num;
  int next_blob_id_available;

public:
  FileHandle ();
  FileHandle (PF::FileHandle pf_file_handle);
  
  Record get (const RID& rid) const;
  RID insert (const char* rec_data);
  void Delete (const RID& rid);
  void update (const Record& rec);

  Page GetPage (PageNum page_num) const;
  Page GetFirstPage () const;

  bool HasNextPage (const Page& page) const;
  Page GetNextPage (const Page& page) const;

  void MakeNewFirstPage ();
  void DoneWritingTo (const Page& page);
  void UnpinPage (const Page& page) const;
  void UpdateHeader ();
  void ForcePages (PageNum pageNum = ALL_PAGES);
};


class Scan
{
private:
  const FileHandle* file_handle;

  AttrType attr_type;
  int attr_len;
  int attr_offset;
  CompOp comp_op;
  const void* value;

  SlotNum current_slot_num;
  Page current_page;
  bool already_unpinned;
  bool scan_underway;

  bool satisfy (const Record& rec) const;

public:
  static const Record end;

  Scan () : already_unpinned (true), scan_underway (false) {}
  ~Scan () {}
  void open (const FileHandle &fileHandle,
             AttrType    attrType,
             int         attrLength,
             int         attrOffset,
             CompOp      compOp,
             const void* value);
  Record next ();
  void close ();
};


class Manager
{
private:
  PF::Manager pfm;

public:
  Manager (PF::Manager &pfm);
  Manager (PF_Manager &pfm);
  
  int MakeBlob (const char* relName, const char *fileName);
  Blob GetBlob (const char* relName, int blob_id);
  void CreateFile (const char* fileName, int record_size);
  void DestroyFile (const char *fileName);
  FileHandle OpenFile (const char *fileName);
  void CloseFile (FileHandle &fileHandle);
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
DECLARE_EXCEPTION (UninitializedRID, "Uninitialized RID.");
DECLARE_EXCEPTION (UninitializedRecord, "Uninitialized record.");
DECLARE_EXCEPTION (NonExistantRid, "RID doesn't exist.");
DECLARE_EXCEPTION (NonExistantRecord, "Record doesn't exist.");
DECLARE_EXCEPTION (BadArgument, "Bad argument.");
}      // namespace error

}      // namespace RM
#endif  // __RM_H
