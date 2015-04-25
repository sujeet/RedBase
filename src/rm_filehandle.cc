#include "rm.h"
#include "rm_page.h"

#include <cstdio>
RM_FileHandle::RM_FileHandle() {}

RM_FileHandle::~RM_FileHandle() {}

RC RM_FileHandle::GetRec(const RID &rid, RM_Record &rec) const
{
  PF_PageHandle page_handle;
  PageNum page_num;
  SlotNum slot_num;
  
  int rc;
  rc = rid.GetPageNum (page_num);
  if (rc != 0) return rc;
  
  rc = rid.GetSlotNum (slot_num);
  if (rc != 0) return rc;
  
  rc = this->pf_filehandle.GetThisPage (page_num, page_handle);
  if (rc != 0) return rc;
  
  char *data;
  rc = page_handle.GetData (data);
  if (rc != 0) return rc;
  
  RM_Page page (data, this->hdr.record_size);

  data = page.GetRecord (slot_num);
  if (data == NULL) return RM_RECORD_NO_EXIST;
  
  rec.data = (char *) malloc (this->hdr.record_size);
  memcpy (rec.data, data, this->hdr.record_size);
  
  return this->pf_filehandle.UnpinPage (page_num);
}

RC RM_FileHandle::InsertRec(const char *pData, RID &rid)
{
  int rc;
  PF_PageHandle page_handle;
  PageNum page_num;
  char *data;
  bool needs_new_page;
  
  needs_new_page = (this->hdr.first_free_page == -1);
  
  if (needs_new_page) {
    // There's no space in the current file,
    // we need to allocate new page.
    rc = this->pf_filehandle.AllocatePage (page_handle);
    if (rc != 0) return rc;
  }
  else {
    rc = pf_filehandle.GetThisPage (hdr.first_free_page, page_handle);
    if (rc != 0) return rc;
  }

  rc = page_handle.GetPageNum (page_num);
  if (rc != 0) return rc;

  rc = page_handle.GetData (data);
  if (rc != 0) return rc;
  
  RM_Page page (data, this->hdr.record_size, needs_new_page);
    
  rid.page_num = page_num;
  rid.slot_num = page.GetFirstEmptySlot ();

  page.InsertRecord (rid.slot_num, pData);

  if (needs_new_page) {
    this->hdr.first_free_page = page_num;
    this->header_modified = true;
  }
  if (page.full ()) {
    this->hdr.first_free_page = page.get_next_free_page ();
    this->header_modified = true;
  }
  return this->pf_filehandle.UnpinPage (page_num);
}

RC RM_FileHandle::DeleteRec(const RID &rid)
{
  PageNum page_num;
  SlotNum slot_num;
  PF_PageHandle page_handle;
  char *data;
  bool was_full;
  int rc;

  rc = rid.GetPageNum (page_num);
  if (rc != 0) return rc;

  rc = rid.GetSlotNum (slot_num);
  if (rc != 0) return rc;

  rc = this->pf_filehandle.GetThisPage (page_num, page_handle);
  if (rc != 0) return rc;

  rc = page_handle.GetData (data);
  if (rc != 0) return rc;

  RM_Page page (data, this->hdr.record_size);
  was_full = page.full ();
  page.DeleteRecord (slot_num);
  if (was_full) {
    page.set_next_free_page (this->hdr.first_free_page);
    this->hdr.first_free_page = page_num;
    this->header_modified = true;
  }
  return this->pf_filehandle.UnpinPage (page_num);
}

RC RM_FileHandle::UpdateRec(const RM_Record &rec)
{
  PageNum page_num;
  SlotNum slot_num;
  PF_PageHandle page_handle;
  char *data;
  RID rid;
  int rc;

  rc = rec.GetRid (rid);
  if (rc != 0) return rc;
  
  rc = rid.GetPageNum (page_num);
  if (rc != 0) return rc;

  rc = rid.GetSlotNum (slot_num);
  if (rc != 0) return rc;

  rc = this->pf_filehandle.GetThisPage (page_num, page_handle);
  if (rc != 0) return rc;

  rc = page_handle.GetData (data);
  if (rc != 0) return rc;

  RM_Page page (data, this->hdr.record_size);

  if (! page.RecordExists (slot_num)) return RM_RECORD_NO_EXIST;
  
  rc = rec.GetData (data);
  if (rc != 0) return rc;
  
  page.InsertRecord (slot_num, data);
  rc = this->pf_filehandle.UnpinPage (page_num);

  return rc;
}

// Forces a page (along with any contents stored in this class)
// from the buffer pool to disk.  Default value forces all pages.
RC RM_FileHandle::ForcePages(PageNum pageNum)
{
  return this->pf_filehandle.ForcePages (pageNum);
}
