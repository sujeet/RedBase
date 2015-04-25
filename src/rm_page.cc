#include "rm_page.h"

static const int page_data_size = PF_PAGE_SIZE - sizeof (rm_page_hdr);

int num_recs (int record_size) {
  return (8 * page_data_size) / (1 + 8 * record_size) - 1;
}

RM_Page::RM_Page (char *data, int record_size, bool wipe)
  :bitmap (num_recs (record_size), (unsigned char*)(data + sizeof (rm_page_hdr))),
   record_capacity (num_recs (record_size))
{
  this->hdr = (rm_page_hdr *) data;
  this->record_capacity = num_recs (record_size);
  this->record_size = record_size;
  this->records = data + sizeof (rm_page_hdr) + bitmap.num_bytes ();

  if (wipe) {
    for (int i = 0; i < this->bitmap.num_bits(); ++i) {
      this->bitmap.clear (i);
    }
    this->set_next_free_page (-1);
    this->hdr->record_count = 0;
  }
}

int RM_Page::get_next_free_page () const
{
  return this->hdr->next_free_page;
}

void RM_Page::DeleteRecord (int slot_number)
{
  this->bitmap.clear (slot_number);
  this->hdr->record_count--;
}

bool RM_Page::full () const
{
  return this->hdr->record_count == this->record_capacity;
}

void RM_Page::set_next_free_page (int page_number)
{
  this->hdr->next_free_page = page_number;
}

char *RM_Page::GetRecord (int slot_number)
{
  if (slot_number >= this->bitmap.num_bits ()) return NULL;
  if (! this->bitmap.get (slot_number)) return NULL;
  return this->records + slot_number * this->record_size;
}

int RM_Page::GetFirstEmptySlot () const
{
  return this->bitmap.getFirstUnset();
}

void RM_Page::InsertRecord (int slot_number, const char *data)
{
  bool slot_was_occupied = this->bitmap.get (slot_number);
  if (! slot_was_occupied) {
    this->hdr->record_count++;
  }
  this->bitmap.set (slot_number);
  memcpy (GetRecord (slot_number), data, this->record_size);
}

bool RM_Page::RecordExists (int slot_number) const
{
  if (slot_number >= this->bitmap.num_bits ()) return false;
  return this->bitmap.get (slot_number);
}
