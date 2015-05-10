#include "rm.h"

using namespace std;

#define ERROR_WRAPPER(expr)                     \
  try {                                         \
    (expr);                                     \
  }                                             \
  catch (exception& e) {                        \
    cout << e.what () << endl;                  \
    return -1;                                  \
  }                                             \
  return 0;

RC RM_Record::GetData(char *&pData) const
{
  pData = this->record.data;
  return 0;
}

RC RM_Record::GetRid (RID &rid) const
{
  rid = this->record.rid;
  return 0;
}

RC RM_FileHandle::GetRec (const RID &rid, RM_Record &rec) const
{
  ERROR_WRAPPER (rec.record = this->handle.get (rid));
}

RC RM_FileHandle::InsertRec (const char *pData, RID &rid)
{
  ERROR_WRAPPER (rid = this->handle.insert (pData));
}

RC RM_FileHandle::DeleteRec (const RID &rid)
{
  ERROR_WRAPPER (this->handle.Delete (rid));
}

RC RM_FileHandle::UpdateRec (const RM_Record &rec)
{
  ERROR_WRAPPER (this->handle.update (rec.record));
}

RC RM_FileHandle::ForcePages (PageNum pageNum)
{
  ERROR_WRAPPER (this->handle.ForcePages (pageNum));
}

RC RM_FileScan::OpenScan  (const RM_FileHandle &fileHandle,
                           AttrType   attrType,
                           int        attrLength,
                           int        attrOffset,
                           CompOp     compOp,
                           void       *value,
                           ClientHint pinHint)
{
  ERROR_WRAPPER (this->scan.open (fileHandle.handle,
                                  attrType,
                                  attrLength,
                                  attrOffset,
                                  compOp,
                                  value));
}

RC RM_FileScan::GetNextRec(RM_Record &rec)
{
  try {
    RM::Record r = this->scan.next ();
    if (r == RM::Scan::end) {
      return RM_EOF; // EOF
    }
    rec.record = r;
  }
  catch (exception& e) {
    cout << e.what () << endl;
    return -1;
  }
  return 0;
}

RC RM_FileScan::CloseScan ()
{
  ERROR_WRAPPER (this->scan.close ());
}

RM_Manager::RM_Manager (PF_Manager &pfm)
  : mgr (pfm) {}

RM_Manager::~RM_Manager () {}

RC RM_Manager::CreateFile (const char *fileName, int recordSize)
{
  ERROR_WRAPPER (this->mgr.CreateFile (fileName, recordSize));
}

RC RM_Manager::DestroyFile (const char *fileName)
{
  ERROR_WRAPPER (this->mgr.DestroyFile (fileName));
}

RC RM_Manager::OpenFile (const char *fileName,
                         RM_FileHandle &fileHandle)
{
  ERROR_WRAPPER (fileHandle.handle = this->mgr.OpenFile (fileName));
}

RC RM_Manager::CloseFile (RM_FileHandle &fileHandle)
{
  ERROR_WRAPPER (this->mgr.CloseFile (fileHandle.handle));
}

void RM_PrintError(RC rc) {}
