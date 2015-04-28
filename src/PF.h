#ifndef __PF_HPP
#define __PF_HPP

#include <cstdio>
#include <exception>
#include <string>
#include <cstring>
#include <cerrno>

#include "pf.h"

using namespace std;

// These are wrappers around the provided PF interfaces
// Goals:
// 1) Do away with return codes, use exceptions instead.
// 2) Use C++ features like namespaces for better naming
//    and organization.

namespace PF
{
class Manager;
class FileHandle;
class PageHandle;

const int kPageSize = PF_PAGE_SIZE;

class Manager 
{
private:
  PF_Manager* manager;
public:
  Manager (PF_Manager& mgr);
  ~Manager () {};

  void CreateFile (const char *fileName);
  void DestroyFile (const char *fileName);
  FileHandle OpenFile (const char *fileName);  

  void CreateFile (const std::string& fileName);
  void DestroyFile (const std::string& fileName);
  FileHandle OpenFile (const std::string& fileName);

  void CloseFile (FileHandle &fileHandle);
};

class FileHandle 
{
  friend class Manager;
private:
  PF_FileHandle* filehandle;
public:
  FileHandle ();
  FileHandle (const FileHandle &fileHandle);
  FileHandle& operator= (const FileHandle &fileHandle);
  ~FileHandle ();

  PageHandle GetFirstPage () const;
  PageHandle GetLastPage () const;

  PageHandle GetNextPage (PageNum current) const; 
  PageHandle GetPrevPage (PageNum current) const;
  PageHandle GetPage (PageNum pageNum) const;  

  PageHandle AllocatePage ();
  void DisposePage (PageNum pageNum);
  void MarkDirty (PageNum pageNum) const;
  void UnpinPage (PageNum pageNum) const;
  void UnpinPage (const PageHandle& page) const;
  void DoneWritingTo (const PageHandle& page) const;
  void ForcePages (PageNum pageNum = ALL_PAGES) const;
};

class PageHandle 
{
  friend class FileHandle;
private:
  PF_PageHandle* pagehandle;
public:
  PageHandle ();
  PageHandle (const PageHandle &pageHandle);
  PageHandle& operator= (const PageHandle &pageHandle);
  ~PageHandle ();
  
  char* GetData() const;
  PageNum GetPageNum () const;
};


#define DECLARE_EXCEPTION(name, message)   \
class name: public exception               \
{                                          \
  virtual const char* what() const throw() \
  {                                        \
    return message;                        \
  }                                        \
};
  
namespace error
{
class SystemError: public exception
{
  virtual const char* what() const throw()
    {
      return strerror (errno);
    }
};
DECLARE_EXCEPTION (PagePinned, "Page pinned in buffer.");
DECLARE_EXCEPTION (PageNotInBuffer, "Page is not in the buffer.");
DECLARE_EXCEPTION (InvalidPageNumber, "Invalid page number.");
DECLARE_EXCEPTION (FileOpen, "File is open.");
DECLARE_EXCEPTION (FileClosed, "File is closed (invalid file descriptor).");
DECLARE_EXCEPTION (PageAlreadyFree, "Page already free.");
DECLARE_EXCEPTION (PageAlreadyPinned, "Page already pinned.");
DECLARE_EXCEPTION (Eof, "End of file.");
DECLARE_EXCEPTION (BufferTooSmall, "Attempting to resize, buffer too small.");

DECLARE_EXCEPTION (NoMemory, "No memory.");
DECLARE_EXCEPTION (NoBufferSpace, "No buffer space.");
DECLARE_EXCEPTION (IncompleteRead, "Incomplete read of page from file.");
DECLARE_EXCEPTION (IncompleteWrite, "Incomplete write of page to file.");
DECLARE_EXCEPTION (IncompleteHeaderRead,
                   "Incomplete read of page from header file.");
DECLARE_EXCEPTION (IncompleteHeaderWrite,
                   "Incomplete write of page to header file.");
DECLARE_EXCEPTION (PageInBuffer, "New page to be allocated already in buffer.");
DECLARE_EXCEPTION (HashNotFound, "Hash table entry not found.");
DECLARE_EXCEPTION (HashPageExists, "Page already in hash table.");
DECLARE_EXCEPTION (InvalidFilename, "Invalid filename.");

}      // namespace error
}      // namespace PF

#endif  // __PF_HPP
