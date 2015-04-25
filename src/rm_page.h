#ifndef RM_PAGE_H
#define RM_PAGE_H

#include <cstdlib>
#include <cstring>
#include "rm.h"
#include "bitmap.h"

typedef struct rm_page_hdr
{
  int next_free_page;
  int record_count;
} rm_page_hdr;

class RM_Page
{
  friend class RM_FileScan;
public:
  RM_Page (char *data, int record_size, bool wipe = false);
  ~RM_Page () {};
  void set_next_free_page (int page_number);
  char *GetRecord (int slot_number);
  int GetFirstEmptySlot () const;
  int get_next_free_page () const;
  void InsertRecord (int slot_number, const char *data);
  void DeleteRecord (int slot_number);
  bool RecordExists (int slot_number) const;
  bool full () const;
private:
  rm_page_hdr *hdr;
  Bitmap bitmap;
  int record_size;
  char *records;
  int record_capacity;
};

#endif  /* RM_PAGE_H */
