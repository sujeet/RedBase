//
// rm_rid.h
//
//   The Record Id interface
//

#ifndef RM_RID_H
#define RM_RID_H

// We separate the interface of RID from the rest of RM because some
// components will require the use of RID but not the rest of RM.

#include "redbase.h"

//
// PageNum: uniquely identifies a page in a file
//
typedef int PageNum;

//
// SlotNum: uniquely identifies a record in a page
//
typedef int SlotNum;

//
// RID: Record id interface
//
class RID {
  friend class RM_FileHandle;
  friend class RM_FileScan;
public:
    RID();                                         // Default constructor
    RID(PageNum pageNum, SlotNum slotNum);

    RC GetPageNum(PageNum &pageNum) const;         // Return page number
    RC GetSlotNum(SlotNum &slotNum) const;         // Return slot number
    bool operator == (const RID& other) const;
    bool operator != (const RID& other) const;

public:
  PageNum page_num;
  SlotNum slot_num;
};

#endif
