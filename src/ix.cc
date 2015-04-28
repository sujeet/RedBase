#include "ix.h"
#include "IX.h"
#include <cassert>

using namespace std;

#define ERROR_WRAPPER(expr)                     \
  try {                                         \
    (expr);                                     \
  }                                             \
  catch (exception& e) {                        \
    return -1;                                  \
  }                                             \
  return 0;

// Insert a new index entry
RC IX_IndexHandle::InsertEntry(void *pData, const RID &rid)
{
  assert (this->handle);
  ERROR_WRAPPER (this->handle->Insert (pData, rid));
}

// Delete a new index entry
RC IX_IndexHandle::DeleteEntry(void *pData, const RID &rid)
{
  assert (this->handle);
  ERROR_WRAPPER (this->handle->Delete (pData, rid));
}

// Force index files to disk
RC IX_IndexHandle::ForcePages()
{
  assert (this->handle);
  ERROR_WRAPPER (this->handle->ForcePages ());
}

RC IX_IndexScan::OpenScan(const IX_IndexHandle &indexHandle,
                          CompOp compOp,
                          void *value,
                          ClientHint  pinHint)
{
  if (pinHint != pinHint) return 1; // To get rid of unused compiler warning
  if (indexHandle.handle == NULL) return -1;
  ERROR_WRAPPER (this->scan = new IX::Scan (*indexHandle.handle, compOp, value));
}


RC IX_IndexScan::GetNextEntry(RID &rid)
{
  try {
    rid = this->scan->next ();
    if (rid == this->scan->end) return IX_EOF;
  }
  catch (exception& e) {
    return -1;
  }
  return 0;
}

RC IX_IndexScan::CloseScan()
{
  ERROR_WRAPPER (delete this->scan);
}

IX_Manager::IX_Manager(PF_Manager &pfm)
{
  this->manager = new IX::Manager (pfm);
}
IX_Manager::~IX_Manager()
{
  delete this->manager;
}

RC IX_Manager::CreateIndex(const char *fileName, int indexNo,
                           AttrType attrType, int attrLength)
{
  ERROR_WRAPPER (this->manager->CreateIndex (fileName,
                                             indexNo,
                                             attrType,
                                             attrLength));
}

RC IX_Manager::DestroyIndex(const char *fileName, int indexNo)
{
  ERROR_WRAPPER (this->manager->DestroyIndex (fileName, indexNo));
}

RC IX_Manager::OpenIndex(const char *fileName, int indexNo,
                         IX_IndexHandle &indexHandle)
{
  ERROR_WRAPPER (
    indexHandle.handle = new IX::IndexHandle (
      this->manager->OpenIndex (fileName, indexNo)
    )
  );
}


RC IX_Manager::CloseIndex(IX_IndexHandle &indexHandle)
{
  if (indexHandle.handle == NULL) return -1;
  ERROR_WRAPPER (this->manager->CloseIndex (*indexHandle.handle));
}


void IX_PrintError(RC rc)
{
  if (rc != rc) return;
}
