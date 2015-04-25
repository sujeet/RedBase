#include <cstdlib>

#include "rm.h"

RM_Record::RM_Record()
{
  this->data = NULL;
}

RM_Record::~RM_Record()
{
  if (this->data != NULL) {
    free (this->data);
  }
}

RC RM_Record::GetData(char *&pData) const
{
  if (this->data == NULL) {
    return RM_RECORD_NO_INIT;
  }
  pData = this->data;
  return 0;
}

RC RM_Record::GetRid (RID &rid) const
{
  if (this->data == NULL) {
    return RM_RECORD_NO_INIT;
  }
  rid = this->rid;
  return 0;
}
