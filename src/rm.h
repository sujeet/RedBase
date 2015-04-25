//
// rm.h
//
//   Record Manager component interface
//
// This file does not include the interface for the RID class.  This is
// found in rm_rid.h
//

#ifndef RM_H
#define RM_H

// Please DO NOT include any files other than redbase.h and pf.h in this
// file.  When you submit your code, the test program will be compiled
// with your rm.h and your redbase.h, along with the standard pf.h that
// was given to you.  Your rm.h, your redbase.h, and the standard pf.h
// should therefore be self-contained (i.e., should not depend upon
// declarations in any other file).

// Do not change the following includes
#include "redbase.h"
#include "rm_rid.h"
#include "pf.h"

//
// RM_Record: RM Record interface
//
class RM_Record {
  friend class RM_FileScan;
  friend class RM_FileHandle;
public:
    RM_Record ();
    ~RM_Record();

    // Return the data corresponding to the record.  Sets *pData to the
    // record contents.
    RC GetData(char *&pData) const;

    // Return the RID associated with the record
    RC GetRid (RID &rid) const;

private:
  char *data;
  RID rid;
};

typedef struct RM_FileHeader
{
  int record_size;
  int first_free_page;
} RM_FileHeader;

//
// RM_FileHandle: RM File interface
//
class RM_FileHandle {
  friend class RM_Manager;
  friend class RM_FileScan;
public:
    RM_FileHandle ();
    ~RM_FileHandle();

    // Given a RID, return the record
    RC GetRec     (const RID &rid, RM_Record &rec) const;

    RC InsertRec  (const char *pData, RID &rid);       // Insert a new record

    RC DeleteRec  (const RID &rid);                    // Delete a record
    RC UpdateRec  (const RM_Record &rec);              // Update a record

    // Forces a page (along with any contents stored in this class)
    // from the buffer pool to disk.  Default value forces all pages.
    RC ForcePages (PageNum pageNum = ALL_PAGES);
private:
  PF_FileHandle pf_filehandle;
  RM_FileHeader hdr;
  bool header_modified;
};

//
// RM_FileScan: condition-based scan of records in the file
//
class RM_FileScan {
public:
    RM_FileScan  ();
    ~RM_FileScan ();

    RC OpenScan  (const RM_FileHandle &fileHandle,
                  AttrType   attrType,
                  int        attrLength,
                  int        attrOffset,
                  CompOp     compOp,
                  void       *value,
                  ClientHint pinHint = NO_HINT); // Initialize a file scan
    RC GetNextRec(RM_Record &rec);               // Get next matching record
    RC CloseScan ();                             // Close the scan
private:
  bool predicate(char *data) const;
  bool open;
  const RM_FileHandle *rm_filehandle;
  AttrType   attrType;
  int        attrLength;
  int        attrOffset;
  CompOp     compOp;
  void       *value;
  int        previous_page_num;
  int        slot_num;
};

//
// RM_Manager: provides RM file management
//
class RM_Manager {
public:
    RM_Manager    (PF_Manager &pfm);
    ~RM_Manager   ();

    RC CreateFile (const char *fileName, int recordSize);
    RC DestroyFile(const char *fileName);
    RC OpenFile   (const char *fileName, RM_FileHandle &fileHandle);

    RC CloseFile  (RM_FileHandle &fileHandle);
private:
  PF_Manager *pfm;
};

//
// Print-error function
//
void RM_PrintError(RC rc);

#define RM_RID_NO_INIT         (START_RM_WARN + 0) // uninitialized RID
#define RM_RECORD_NO_INIT      (START_RM_WARN + 1) // uninitialized RM_Record
#define RM_RECORD_NO_EXIST     (START_RM_WARN + 2) // non-existing record
#define RM_EOF                 (START_RM_WARN + 3) // end of file
#define RM_INVALID_RECORD_SIZE (START_RM_WARN + 4) // end of file
#define RM_UNOPENED_FILESCAN   (START_RM_WARN + 5) // interacting with filescan
                                                   // without opening one
#define RM_LASTWARN            RM_UNOPENED_FILESCAN

#define RM_INVALID_SCAN_PARAMS (START_RM_ERR - 0) // invalid parms for scan
#define RM_LASTERROR           RM_INVALID_SCAN_PARAMS

#endif
