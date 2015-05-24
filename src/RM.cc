#include <cstring>
#include <cassert>

#include "RM.h"

namespace RM
{

Record::Record ()
{
  memset (this->data_, 0, PF::kPageSize);
  this->data = this->data_;
}

Record::Record (const Record& other)
{
  memcpy (this->data_, other.data_, PF::kPageSize);
  this->rid = other.rid;
  this->data = this->data_;
}

Record& Record::operator = (const Record& other)
{
  memcpy (this->data_, other.data_, PF::kPageSize);
  this->rid = other.rid;
  this->data = this->data_;
  return (*this);
}
  
bool Record::operator == (const Record& other) const
{
  return (this->rid == other.rid);
}

bool Record::operator != (const Record& other) const
{
  return not (*this == other);
}

Manager::Manager (PF::Manager &pfm)
  : pfm (pfm) {}

Manager::Manager (PF_Manager &pfm)
  : pfm (pfm) {}
  
void Manager::CreateFile (const char* fileName, int record_size)
{
  if (fileName == NULL) throw error::BadArgument ();
  if (record_size <= 0) throw error::BadArgument ();
  if (record_size > PF::kPageSize) throw error::BadArgument ();

  // Create header
  this->pfm.CreateFile (fileName);
  auto pf_file = this->pfm.OpenFile (fileName);
  auto pf_hdr_pg = pf_file.AllocatePage ();
  HeaderPage hdr_pg (pf_hdr_pg);
  hdr_pg.clear ();
  hdr_pg.SetRecordSize (record_size);

  // create first page
  auto pf_first_pg = pf_file.AllocatePage ();
  Page first_pg (pf_first_pg, record_size);
  first_pg.clear ();

  // Reference the first page from the header page.
  hdr_pg.SetFirstPageNum (pf_first_pg.GetPageNum ());

  // Done with both the pages.
  pf_file.DoneWritingTo (pf_hdr_pg);
  pf_file.DoneWritingTo (pf_first_pg);
  this->pfm.CloseFile (pf_file);
}

void Manager::DestroyFile (const char *fileName)
{
  if (fileName == NULL) throw error::BadArgument ();

  this->pfm.DestroyFile (fileName);
}

FileHandle Manager::OpenFile (const char *fileName)
{
  if (fileName == NULL) throw error::BadArgument ();

  FileHandle fh (this->pfm.OpenFile (fileName));
  return fh;
}

void Manager::CloseFile (FileHandle &fileHandle)
{
  if (fileHandle.header_modified) {
    fileHandle.UpdateHeader ();
  }
  this->pfm.CloseFile (fileHandle.pf_file_handle);
}

FileHandle::FileHandle ()
  : record_size (0), header_modified (false) {} 

FileHandle::FileHandle (PF::FileHandle pf_file_handle)
  : pf_file_handle (pf_file_handle)
{
  auto pf_header = this->pf_file_handle.GetFirstPage();
  HeaderPage header (pf_header);

  this->record_size = header.GetRecordSize ();
  this->first_page_num = header.GetFirstPageNum ();
  this->header_modified = false;

  this->pf_file_handle.UnpinPage (pf_header);
}

Record FileHandle::get (const RID& rid) const
{
  auto page = this->GetPage (rid.page_num);
  auto rec = page.get (rid.slot_num);
  this->UnpinPage (page);
  return rec;
}

RID FileHandle::insert (const char* rec_data)
{
  if (rec_data == NULL) throw error::BadArgument ();

  // We maintain the invariant that the first page
  // always has space for a new record.
  auto page = this->GetFirstPage ();
  SlotNum slot_num = page.insert (rec_data);
  RID rid (this->first_page_num, slot_num);
  if (page.full()) {
    this->MakeNewFirstPage ();
  }
  this->DoneWritingTo (page);
  return rid;
}

void FileHandle::Delete (const RID& rid)
{
  auto page = this->GetPage (rid.page_num);
  try {
    page.Delete (rid.slot_num);
  }
  catch (exception& e) {
    this->UnpinPage (page);
    throw;
  }
  this->DoneWritingTo (page);
}

void FileHandle::update (const Record& rec)
{
  auto page = this->GetPage (rec.rid.page_num);
  try {
    page.update (rec);
  }
  catch (exception& e) {
    this->UnpinPage (page);
    throw;
  }
  this->DoneWritingTo (page);
}

Page FileHandle::GetFirstPage () const
{
  return this->GetPage (this->first_page_num);
}

Page FileHandle::GetPage (PageNum page_num) const
{
  auto pf_page = this->pf_file_handle.GetPage (page_num);
  Page page (pf_page, this->record_size);
  return page;
}

void FileHandle::MakeNewFirstPage ()
{
  auto pf_page = this->pf_file_handle.AllocatePage ();
  Page new_first_page (pf_page, this->record_size);
  new_first_page.SetNextPageNum (this->first_page_num);
  this->header_modified = true;
  this->first_page_num = new_first_page.GetPageNum ();
  this->DoneWritingTo (new_first_page);
}

void FileHandle::DoneWritingTo (const Page& page)
{
  this->pf_file_handle.DoneWritingTo (page.pf_page);
}

void FileHandle::UnpinPage (const Page& page) const
{
  this->pf_file_handle.UnpinPage (page.pf_page);
}

void FileHandle::UpdateHeader ()
{
  assert (this->header_modified);

  auto pf_header = this->pf_file_handle.GetFirstPage ();
  HeaderPage header_page (pf_header);
  header_page.SetFirstPageNum (this->first_page_num);
  this->pf_file_handle.DoneWritingTo (pf_header);

  this->header_modified = false;
}

bool FileHandle::HasNextPage (const Page& page) const
{
  return (page.hdr->next_page != INVALID);
}

Page FileHandle::GetNextPage (const Page& page) const
{
  assert (this->HasNextPage (page));
  
  return this->GetPage (page.hdr->next_page);
}

void FileHandle::ForcePages (PageNum pageNum)
{
  this->pf_file_handle.ForcePages (pageNum);
}

Page::Page (PF::PageHandle& pf_page, int record_size)
  : pf_page (pf_page)
{
  this->record_size = record_size;

  char* data = pf_page.GetData ();

  this->hdr = (PageHdr*) data;
  data += sizeof (PageHdr);

  // if there are n records, we need (n+7)/8 bytes for the bitmap
  // (n+7)/8 + n*rec_size <= available_bytes
  //
  //              8 * available_bytes - 7
  //      n  <=  -------------------------
  //               1 + 8 * rec_size

  int available_bytes = PF::kPageSize - sizeof (PageHdr);
  this->max_num_records = (8*available_bytes - 7) / (1 + 8*record_size);
  new (&this->bitmap) Bitmap (this->max_num_records, data);
  data += this->bitmap.num_bytes ();

  this->records = data;
}

PageNum Page::GetPageNum () const
{
  return this->pf_page.GetPageNum ();
}

void Page::clear ()
{
  this->hdr->num_records = 0;
  this->hdr->next_page = INVALID;
  this->bitmap.clear_all ();
}

Record Page::get (SlotNum slot_num) const
{
  assert (slot_num < this->max_num_records);
  assert (this->bitmap.get (slot_num));

  Record record;
  record.rid.page_num = this->pf_page.GetPageNum ();
  record.rid.slot_num = slot_num;
  memcpy (record.data,
          this->records + slot_num * this->record_size,
          this->record_size);
  return record;
}

SlotNum Page::insert (const char* rec_data)
{
  assert (not this->full());

  SlotNum slot_num = this->bitmap.getFirstUnset ();
  this->bitmap.set (slot_num);
  this->hdr->num_records++;
  memcpy (this->records + slot_num * this->record_size,
          rec_data,
          this->record_size);
  return slot_num;
}

void Page::Delete (SlotNum slot_num)
{
  if (not this->bitmap.get (slot_num)) {
    throw error::NonExistantRecord ();
  }

  this->bitmap.clear (slot_num);
  this->hdr->num_records--;
}

void Page::update (const Record& rec)
{
  if (not this->bitmap.get (rec.rid.slot_num)) {
    throw error::NonExistantRecord ();
  }

  memcpy (this->records + rec.rid.slot_num * this->record_size,
          rec.data,
          this->record_size);
}

void Page::SetNextPageNum (PageNum next_page_num)
{
  this->hdr->next_page = next_page_num;
}

bool Page::full () const
{
  assert (this->hdr->num_records <= this->max_num_records);

  return this->hdr->num_records == this->max_num_records;
}

HeaderPage::HeaderPage (PF::PageHandle& pf_page)
  : pf_page (pf_page) {}

void HeaderPage::clear ()
{
  this->SetFirstPageNum (INVALID);
}

void HeaderPage::SetRecordSize (int record_size)
{
  * (int *) this->pf_page.GetData () = record_size;
}

void HeaderPage::SetFirstPageNum (PageNum first_page_num)
{
  // in a header page, first record size is stored,
  // then first page number is stored.
  char* data = this->pf_page.GetData ();
  data += sizeof (int);                // get past the record size.
  *(PageNum *) data = first_page_num;  // set first page nuber.
}

PageNum HeaderPage::GetFirstPageNum () const
{
  char* data = this->pf_page.GetData ();
  data += sizeof (int);                // get past the record size.
  return *(PageNum*) data;
}

int HeaderPage::GetRecordSize () const
{
  return *(int*)(this->pf_page.GetData ());
}

const Record Scan::end;

void Scan::open (const FileHandle &fileHandle,
                 AttrType    attrType,
                 int         attrLength,
                 int         attrOffset,
                 CompOp      compOp,
                 const void* value)
{
  if ((this->scan_underway) ||
      (attrLength < 0) ||
      (attrType != INT && attrType != FLOAT && attrType != STRING) ||
      (compOp != NO_OP && compOp != EQ_OP &&
       compOp != NE_OP && compOp != LT_OP &&
       compOp != GT_OP && compOp != LE_OP && compOp != GE_OP) ||
      (attrType == INT && attrLength != 4) ||
      (attrType == FLOAT && attrLength != 4) ||
      (attrType == STRING && attrLength > MAXSTRINGLEN) ||
      (compOp == NO_OP && value != NULL) ||
      (compOp != NO_OP && value == NULL) ||
      (fileHandle.record_size < attrLength + attrOffset))
    throw error::BadArgument ();

  this->file_handle = &fileHandle;
  this->attr_type = attrType;
  this->attr_len = attrLength;
  this->attr_offset = attrOffset;
  this->comp_op = compOp;
  this->value = value;
  this->current_slot_num = 0;
  this->current_page = fileHandle.GetFirstPage ();
  this->already_unpinned = false;
  this->scan_underway = true;
}

bool Scan::satisfy (const Record& rec) const
{
  if (this->comp_op == NO_OP) return true;

  if (this->attr_type == INT) {
    int attribute = *(int*)(rec.data + this->attr_offset);
    int value = *(int *) this->value;
    switch (this->comp_op) {
      case LT_OP: return attribute < value;
      case GT_OP: return attribute > value;
      case EQ_OP: return attribute == value;
      case LE_OP: return attribute <= value;
      case GE_OP: return attribute >= value;
      case NE_OP: return attribute != value;
      default: return true;
    }
  }
  else if (this->attr_type == FLOAT) {
    float attribute = *(float*)(rec.data + this->attr_offset);
    float value = *(float*) this->value;
    switch (this->comp_op) {
      case LT_OP: return attribute < value;
      case GT_OP: return attribute > value;
      case EQ_OP: return attribute == value;
      case LE_OP: return attribute <= value;
      case GE_OP: return attribute >= value;
      case NE_OP: return attribute != value;
      default: return true;
    }
  }
  else {
    const char* attribute = rec.data + this->attr_offset;
    char* value = (char*) this->value;
    switch (this->comp_op) {
      case LT_OP: return strncmp (attribute, value, this->attr_len) < 0;
      case GT_OP: return strncmp (attribute, value, this->attr_len) > 0;
      case EQ_OP: return strncmp (attribute, value, this->attr_len) == 0;
      case LE_OP: return strncmp (attribute, value, this->attr_len) <= 0;
      case GE_OP: return strncmp (attribute, value, this->attr_len) >= 0;
      case NE_OP: return strncmp (attribute, value, this->attr_len) != 0;
      default: return true;
    }
  }
}

Record Scan::next ()
{
  if (this->current_slot_num == this->current_page.max_num_records) {
    // We have returned all the records from this page.

    if (not this->file_handle->HasNextPage (this->current_page)) {
      // There are no more pages.
      this->file_handle->UnpinPage (this->current_page);
      this->already_unpinned = true;
      return end;
    }

    this->file_handle->UnpinPage (this->current_page);
    this->current_page = this->file_handle->GetNextPage (this->current_page);
    this->current_slot_num = 0;
  }

  while (this->current_slot_num < this->current_page.max_num_records) {
    if (not this->current_page.bitmap.get (this->current_slot_num)) {
      this->current_slot_num ++;
      continue;
    }
    Record rec = this->current_page.get (this->current_slot_num);
    if (not this->satisfy (rec)) {
      this->current_slot_num ++;
      continue;
    }
    else {
      this->current_slot_num ++;
      return rec;
    }
  }

  return this->next ();
}

void Scan::close ()
{
  this->scan_underway = false;
  if (not this->already_unpinned)
    this->file_handle->UnpinPage (this->current_page);
}

}  // namespace RM
