#include "rm.h"
#include "rm_page.h"

#include <cstdio>

RM_FileScan::RM_FileScan () {}
RM_FileScan::~RM_FileScan () {}

RC RM_FileScan::OpenScan (const RM_FileHandle &fileHandle,
                          AttrType attrType,
                          int attrLength,
                          int attrOffset,
                          CompOp compOp,
                          void *value,
                          ClientHint pinHint)
{
  this->open = true;
  if ((attrType != STRING && attrLength != 4) ||
      (attrLength + attrOffset > fileHandle.hdr.record_size)) {
    return RM_INVALID_SCAN_PARAMS;
  }

  this->rm_filehandle = &fileHandle;
  this->attrType = attrType;
  this->attrLength = attrLength;
  this->attrOffset = attrOffset;
  this->compOp = compOp;
  this->value = value;

  int rc;
  PF_PageHandle page_handle;
  rc = this->rm_filehandle->pf_filehandle.GetFirstPage (page_handle);
  if (rc != 0) return rc;
  
  rc = page_handle.GetPageNum (this->previous_page_num);
  if (rc != 0) return rc;

  this->slot_num = 0;
  return 0;
}

RC RM_FileScan::GetNextRec (RM_Record &rec)
{
  if (! this->open) {
    return RM_UNOPENED_FILESCAN;
  }
  int rc;
  PF_PageHandle page_handle;
  char *data;

  while (this->rm_filehandle->pf_filehandle.GetNextPage (
           this->previous_page_num,
           page_handle
         ) == 0) {

    rc = page_handle.GetData (data);
    if (rc != 0) return rc;

    RM_Page page (data, this->rm_filehandle->hdr.record_size);

    while (this->slot_num < page.record_capacity) {
      data = page.GetRecord (this->slot_num++);
      if (data == NULL || ! this->predicate (data)) continue;

      if (rec.data == NULL) {
        rec.data = (char *) malloc (this->rm_filehandle->hdr.record_size);
      }
      memcpy (rec.data, data, this->rm_filehandle->hdr.record_size);
      page_handle.GetPageNum (rec.rid.page_num);
      rec.rid.slot_num = this->slot_num - 1;

      return 0;
    }
    
    this->slot_num = 0;
    rc = page_handle.GetPageNum (this->previous_page_num);
    if (rc != 0) return rc;
  }

  return RM_EOF;
}

RC RM_FileScan::CloseScan ()
{
  if (! this->open) {
    return RM_UNOPENED_FILESCAN;
  }
  this->open = false;
  return 0;
}

bool RM_FileScan::predicate (char *data) const
{
  if (value == NULL) return true;

  if (attrType == INT) {
    int int_attr = *((int *)(data + attrOffset));
    int int_value = *((int *) value);
    switch (compOp) {
    case EQ_OP: return int_attr == int_value;
    case LT_OP: return int_attr < int_value;
    case GT_OP: return int_attr > int_value;
    case LE_OP: return int_attr <= int_value;
    case GE_OP: return int_attr >= int_value;
    case NE_OP: return int_attr != int_value;
    case NO_OP: return true;
    }
  }
  else if (attrType == FLOAT) {
    float float_attr = *((float *)(data + attrOffset));
    float float_value = *((float *) value);
    switch (compOp) {
    case EQ_OP: return float_attr == float_value;
    case LT_OP: return float_attr < float_value;
    case GT_OP: return float_attr > float_value;
    case LE_OP: return float_attr <= float_value;
    case GE_OP: return float_attr >= float_value;
    case NE_OP: return float_attr != float_value;
    case NO_OP: return true;
    }
  }
  else {
    char *str_attr = data + attrOffset;
    char *str_value = (char *) value;
    switch (compOp) {
    case EQ_OP: return strncmp (str_attr, str_value, attrLength) == 0;
    case LT_OP: return strncmp (str_attr, str_value, attrLength) < 0;
    case GT_OP: return strncmp (str_attr, str_value, attrLength) > 0;
    case LE_OP: return strncmp (str_attr, str_value, attrLength) <= 0;
    case GE_OP: return strncmp (str_attr, str_value, attrLength) >= 0;
    case NE_OP: return strncmp (str_attr, str_value, attrLength) != 0;
    case NO_OP: return true;
    }
  }
  return false;
}
