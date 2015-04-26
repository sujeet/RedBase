#include "PF.hpp"

#define HANDLE_ERROR(stmt)   \
  int rc = (stmt);           \
  if (rc != 0) PF_THROW (rc);

#define PF_THROW(code)                                             \
  switch(code) {                                                   \
  case PF_PAGEPINNED      : throw PagePinned (); break;            \
  case PF_PAGENOTINBUF    : throw PageNotInBuffer (); break;       \
  case PF_INVALIDPAGE     : throw InvalidPageNumber (); break;     \
  case PF_FILEOPEN        : throw FileOpen (); break;              \
  case PF_CLOSEDFILE      : throw FileClosed (); break;            \
  case PF_PAGEFREE        : throw PageAlreadyFree (); break;       \
  case PF_PAGEUNPINNED    : throw PageAlreadyPinned (); break;     \
  case PF_EOF             : throw EOF (); break;                   \
  case PF_TOOSMALL        : throw BufferTooSmall (); break;        \
                                                                   \
  case PF_NOMEM           : throw NoMemory (); break;              \
  case PF_NOBUF           : throw NoBufferSpace (); break;         \
  case PF_INCOMPLETEREAD  : throw IncompleteRead (); break;        \
  case PF_INCOMPLETEWRITE : throw IncompleteWrite (); break;       \
  case PF_HDRREAD         : throw IncompleteHeaderRead (); break;  \
  case PF_HDRWRITE        : throw IncompleteHeaderWrite (); break; \
                                                                   \
  case PF_PAGEINBUF       : throw PageInBuffer (); break;          \
  case PF_HASHNOTFOUND    : throw HashNotFound (); break;          \
  case PF_HASHPAGEEXIST   : throw HashPageExists (); break;        \
  case PF_INVALIDNAME     : throw InvalidFilename (); break;       \
  }

namespace PF
{

using namespace error;

Manager::Manager () 
{
  this->manager = new PF_Manager ();
}

Manager::~Manager () 
{
  delete this->manager;
}

void Manager::CreateFile (const char *fileName) 
{
  HANDLE_ERROR (this->manager->CreateFile (fileName));
}

void Manager::DestroyFile (const char *fileName)
{
  HANDLE_ERROR (this->manager->DestroyFile (fileName));
}

FileHandle Manager::OpenFile (const char *fileName)
{
  FileHandle filehandle;
  HANDLE_ERROR (this->manager->OpenFile (fileName, *filehandle.filehandle));
  return filehandle;
}

void Manager::CloseFile (FileHandle &filehandle)
{
  HANDLE_ERROR (this->manager->CloseFile (*filehandle.filehandle));
}

FileHandle::FileHandle ()
{
  this->filehandle = new PF_FileHandle ();
}

FileHandle::FileHandle (const FileHandle &fileHandle)
{
  this->filehandle = new PF_FileHandle (*fileHandle.filehandle);
}

FileHandle& FileHandle::operator= (const FileHandle &fileHandle)
{
  *this->filehandle = *fileHandle.filehandle;
  return (*this);
}

FileHandle::~FileHandle ()
{
  delete this->filehandle;
}

PageHandle FileHandle::GetFirstPage () const
{
  PageHandle pagehandle;
  HANDLE_ERROR (this->filehandle->GetFirstPage (*pagehandle.pagehandle));
  return pagehandle;
}

PageHandle FileHandle::GetLastPage () const
{
  PageHandle pagehandle;
  HANDLE_ERROR (this->filehandle->GetLastPage (*pagehandle.pagehandle));
  return pagehandle;
}


PageHandle FileHandle::GetNextPage (PageNum current) const
{
  PageHandle pagehandle;
  HANDLE_ERROR (this->filehandle->GetNextPage (current,
                                               *pagehandle.pagehandle));
  return pagehandle;
}
 
PageHandle FileHandle::GetPrevPage (PageNum current) const
{
  PageHandle pagehandle;
  HANDLE_ERROR (this->filehandle->GetPrevPage (current,
                                               *pagehandle.pagehandle));
  return pagehandle;
}

PageHandle FileHandle::GetPage (PageNum pageNum) const
{
  PageHandle pagehandle;
  HANDLE_ERROR (this->filehandle->GetThisPage (pageNum,
                                               *pagehandle.pagehandle));
  return pagehandle;
}
  

PageHandle FileHandle::AllocatePage ()
{
  PageHandle pagehandle;
  HANDLE_ERROR (this->filehandle->AllocatePage (*pagehandle.pagehandle));
  return pagehandle;
}

void FileHandle::DisposePage (PageNum pageNum)
{
  HANDLE_ERROR (this->filehandle->DisposePage (pageNum));
}

void FileHandle::MarkDirty (PageNum pageNum) const
{
  HANDLE_ERROR (this->filehandle->MarkDirty (pageNum));
}

void FileHandle::UnpinPage (PageNum pageNum) const
{
  HANDLE_ERROR (this->filehandle->UnpinPage (pageNum));
}

void FileHandle::ForcePages (PageNum pageNum) const
{
  HANDLE_ERROR (this->filehandle->ForcePages (pageNum));
}

PageHandle::PageHandle ()
{
  this->pagehandle = new PF_PageHandle ();
}

PageHandle::PageHandle (const PageHandle &pageHandle)
{
  this->pagehandle = new PF_PageHandle (*pageHandle.pagehandle);
}

PageHandle& PageHandle::operator= (const PageHandle &pageHandle)
{
  *this->pagehandle = *pageHandle.pagehandle;
  return (*this);
}

PageHandle::~PageHandle ()
{
  delete this->pagehandle;
}

char* PageHandle::GetData () const
{
  char *data;
  HANDLE_ERROR (this->pagehandle->GetData (data));
  return data;
}

PageNum PageHandle::GetPageNum () const
{
  PageNum pageNum;
  HANDLE_ERROR (this->pagehandle->GetPageNum (pageNum));
  return pageNum;
}
}
