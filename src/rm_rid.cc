#include "rm.h"
#include "rm_rid.h"

const int RM_RID_NO_INIT = -99;

RID::RID():page_num(INVALID), slot_num(INVALID){}

RID::RID(PageNum pageNum,
         SlotNum slotNum):page_num(pageNum), slot_num(slotNum){}

RC RID::GetPageNum(PageNum &pageNum) const
{
  if (this->page_num == INVALID || this->slot_num == INVALID) {
    return RM_RID_NO_INIT;
  }
  pageNum = this->page_num;
  return 0;
}

RC RID::GetSlotNum(SlotNum &slotNum) const
{
  if (this->page_num == INVALID || this->slot_num == INVALID) {
    return RM_RID_NO_INIT;
  }
  slotNum = this->slot_num;
  return 0;
}

bool RID::operator == (const RID& other) const
{
  return (this->page_num == other.page_num &&
          this->slot_num == other.slot_num);
}

bool RID::operator != (const RID& other) const
{
  return !((*this) == other);
}
