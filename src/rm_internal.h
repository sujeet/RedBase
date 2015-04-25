#ifndef RM_INTERNAL_H
#define RM_INTERNAL_H

#include <cstdlib>
#include <cstring>
#include "rm.h"

typedef struct rm_page_hdr
{
  int next_free_page;
  int prev_free_page;
  int record_count;
} rm_page_hdr;

typedef struct rm_page
{
  rm_page_hdr hdr;
  char data [PF_PAGE_SIZE - sizeof (rm_page_hdr)];
} rm_page;

void rm_page_init (rm_page *page);

// Copies the record at record_idx
// into the record buffer, if the record exists.
// Returns true on success
//         false if the record doesn't exist.
bool rm_page_get_record (const rm_page *page,
                         const int record_idx,
                         const int record_size,
                         char *record_buffer);

void rm_page_set_record (rm_page *page,
                         const int record_idx,
                         const int record_size,
                         const char *record_buffer);

void rm_page_delete_record (rm_page *page,
                            const int record_idx,
                            const int record_size);

#endif  /* RM_INTERNAL_H */
