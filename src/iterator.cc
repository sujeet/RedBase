#include <cassert>
#include <cstring>

#include "iterator.h"

RelIterator::RelIterator (const char* rel_name,
                          const vector<condition>& conditions,
                          RM::Manager* rmm,
                          IX::Manager* ixm,
                          SM::Manager* smm)
  : rmm (rmm), ixm (ixm), smm (smm),
    conditions (conditions), using_index_scan (false),
    rel_name (rel_name)
{
  this->rel = this->rmm->OpenFile (rel_name);
  this->tuple_buffer = new char [this->tuple_size ()];

  // Check whether we can use an index scan instead.
  auto attr_recs = this->smm->GetAttributes (rel_name);
  this->scan.open (this->rel, INT, 4, 0, NO_OP, NULL); return;
  for (unsigned int i = 0; i < attr_recs.size(); ++i) {
    Attribute *attr = (Attribute *) attr_recs[i].data;
    if (attr->index_num == -1) continue;
    for (unsigned int j = 0; j < conditions.size(); ++j) {
      if (conditions [j].offset1 == attr->offset and
          not conditions [j].has_rhs_attr) {
        this->index = this->ixm->OpenIndex (rel_name, attr->index_num);
        this->index_scan_condition = conditions [j];
        this->index_scan = new IX::Scan (this->index,
                                         conditions[j].comp_op,
                                         conditions[j].value);
        this->using_index_scan = true;
        return;
      }
    }
  }
}

RelIterator::~RelIterator ()
{
  if (not using_index_scan) this->scan.close ();
  else {
    delete this->index_scan;
    this->ixm->CloseIndex (this->index);
  }
  this->rmm->CloseFile (this->rel);
  delete [] this->tuple_buffer;
}

void RelIterator::open () {}
void RelIterator::close () {}

void RelIterator::reset ()
{
  if (not using_index_scan) {
    this->scan.close ();
    this->scan.open (this->rel, INT, 4, 0, NO_OP, NULL);
  }
  else {
    delete this->index_scan;
    this->index_scan = new IX::Scan (this->index,
                                     this->index_scan_condition.comp_op,
                                     this->index_scan_condition.value);
  }
}

RID RelIterator::rid ()
{
  return this->rid_;
}


bool is_long (const Blob& blob)
{
  return blob.size() > 1000;
}

char* RelIterator::next ()
{
  RM::Record rec;
  if (not using_index_scan) {
    while ((rec = this->scan.next ()) != this->scan.end) {
      bool match_found = true;
      for (unsigned int i = 0; i < this->conditions.size (); ++i) {
        if (conditions[i].attr_type != BLOB)
          match_found = match_found and this->conditions[i].satisfies (rec.data);
        else {
          int blob_id = *(int*)(rec.data + conditions[i].offset1);
          Blob b = this->rmm->GetBlob (this->rel_name, blob_id);
          match_found = match_found and is_long (b);
        }
      }
      if (match_found) {
        memcpy (this->tuple_buffer, rec.data, this->tuple_size ());
        this->rid_ = rec.rid;
        return this->tuple_buffer;
      }
    }
  }
  else {
    RID rid;
    while ((rid = this->index_scan->next ()) != this->index_scan->end) {
      cout << rid.page_num << " " << rid.slot_num << endl;
      bool match_found = true;
      rec = this->rel.get (rid);
      for (unsigned int i = 0; i < this->conditions.size (); ++i) {
        match_found = match_found and this->conditions[i].satisfies (rec.data);
      }
      if (match_found) {
        memcpy (this->tuple_buffer, rec.data, this->tuple_size ());
        this->rid_ = rec.rid;
        return this->tuple_buffer;
      }
    }
  }
  return NULL;
}

int RelIterator::tuple_size () const
{
  return this->rel.record_size;
}

CompositeIterator::CompositeIterator (Iterator* iter1,
                                      Iterator* iter2,
                                      const vector<condition>& conditions)
  : iter1 (iter1), iter2 (iter2), conditions (conditions)
{
  this->tuple_buffer = new char [iter1->tuple_size () + iter2->tuple_size ()];
}

CompositeIterator::~CompositeIterator ()
{
  delete this->iter1;
  delete this->iter2;
  
  delete [] this->tuple_buffer;
}

void CompositeIterator::open ()
{
  this->iter1->open ();
  this->iter2->open ();
  this->tuple1 = this->iter1->next ();
}

void CompositeIterator::close ()
{
  this->iter1->close ();
  this->iter2->close ();
}

void CompositeIterator::reset ()
{
  this->iter1->reset ();
  this->iter2->reset ();
}

char* CompositeIterator::next ()
{
  char* tuple2;
  while (tuple1 != NULL) {
    while ((tuple2 = this->iter2->next ()) != NULL) {
      bool match_found = true;
      for (unsigned int i = 0; i < this->conditions.size (); ++i) {
        match_found = match_found and this->conditions[i].satisfies (
          tuple1,
          tuple2
        );
      }
      if (match_found) {
        memcpy (this->tuple_buffer, tuple1, this->iter1->tuple_size ());
        memcpy (this->tuple_buffer + this->iter1->tuple_size (),
                tuple2,
                this->iter2->tuple_size ());
        return this->tuple_buffer;
      }
    }
    this->iter2->reset ();
    tuple1 = this->iter1->next ();
  }
  return NULL;
}

int CompositeIterator::tuple_size () const
{
  return this->iter1->tuple_size () + this->iter2->tuple_size ();
}

bool condition::satisfies (char* tuple) const
{
  if (this->attr_type == INT) {
    const int attribute = *(const int*)(tuple + this->offset1);
    int value;
    if (not this->has_rhs_attr)
      value = *(int *) this->value;
    else
      value = *(const int*)(tuple + this->offset2);
      
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
    const float attribute = *(const float*)(tuple + this->offset1);
    float value;
    if (not this->has_rhs_attr)
      value = *(float *) this->value;
    else
      value = *(const float*)(tuple + this->offset2);

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
    char* attribute = tuple + this->offset1;
    char* value;
    if (not this->has_rhs_attr)
      value = (char *) this->value;
    else
      value = tuple + this->offset2;

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
  assert (false);
  return false;
}

bool condition::satisfies (char* tuple1, char* tuple2) const
{
  assert (this->has_rhs_attr);

  if (this->attr_type == INT) {
    const int attribute = *(const int*)(tuple1 + this->offset1);
    const int value = *(const int*)(tuple2 + this->offset2);
      
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
    const float attribute = *(const float*)(tuple1 + this->offset1);
    const float value = *(const float*)(tuple2 + this->offset2);

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
    char* attribute = tuple1 + this->offset1;
    char* value = tuple2 + this->offset2;

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
  assert (false);
  return false;
}
