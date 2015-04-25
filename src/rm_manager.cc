#include <cstdio>
#include <cstring>
#include "rm.h"
#include "rm_page.h"

RM_Manager::RM_Manager (PF_Manager &pfm)
{
  this->pfm = &pfm;
}

RM_Manager::~RM_Manager () {};

RC RM_Manager::CreateFile (const char *fileName, int recordSize)
{
  if (recordSize < 0 ||
      recordSize + sizeof (rm_page_hdr) + 1 > PF_PAGE_SIZE) {
    return RM_INVALID_RECORD_SIZE;
  }

  // Create pf file
  int rc = this->pfm->CreateFile (fileName);
  if (rc != 0) return rc;
  
  // Open pf file
  RM_FileHandle file_handle;
  rc = this->pfm->OpenFile (fileName, file_handle.pf_filehandle);
  if (rc != 0) return rc;
  
  // Add a header page
  PF_PageHandle header_page;
  rc = file_handle.pf_filehandle.AllocatePage (header_page);
  if (rc != 0) return rc;

  int header_page_num;
  header_page.GetPageNum (header_page_num);
  rc = file_handle.pf_filehandle.UnpinPage (header_page_num);
  if (rc != 0) return rc;

  // Prepare header data
  file_handle.hdr.record_size = recordSize;
  file_handle.hdr.first_free_page = -1;
  file_handle.header_modified = true;

  // Write header data and close the file.
  return this->CloseFile (file_handle);
}

RC RM_Manager::DestroyFile (const char *fileName)
{
  return this->pfm->DestroyFile (fileName);
}

RC RM_Manager::OpenFile (const char *fileName, RM_FileHandle &fileHandle)
{
  int rc = this->pfm->OpenFile (fileName, fileHandle.pf_filehandle);
  if (rc != 0) return rc;
  

  PF_PageHandle page_handle;
  rc = fileHandle.pf_filehandle.GetFirstPage (page_handle);
  if (rc != 0) return rc;
  
  char *data;
  rc = page_handle.GetData (data);
  if (rc != 0) return rc;
  
  fileHandle.hdr = *((RM_FileHeader *) (data));
  fileHandle.header_modified = false;
  
  int header_page_number;
  page_handle.GetPageNum (header_page_number);
  return fileHandle.pf_filehandle.UnpinPage (header_page_number);
}

RC RM_Manager::CloseFile (RM_FileHandle &fileHandle)
{
  if (fileHandle.header_modified) {
    // Header's been modified, need to write that to the page
    // before closing the file.
    int rc;
    char *data;
    PF_PageHandle page_handle;
    int page_num;

    rc = fileHandle.pf_filehandle.GetFirstPage (page_handle);
    if (rc != 0) return rc;
    
    rc = page_handle.GetData (data);
    if (rc != 0) return rc;
    
    memcpy (data, &fileHandle.hdr, sizeof (RM_FileHeader));

    page_handle.GetPageNum (page_num);

    rc = fileHandle.pf_filehandle.UnpinPage (page_num);
    if (rc != 0) return rc;

    fileHandle.header_modified = false;
  }

  return this->pfm->CloseFile (fileHandle.pf_filehandle);
}
