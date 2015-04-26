#include <cassert>
#include <cstring>

#include "Array.h"

ArrayElem::ArrayElem (AttrType type, int len, char* data):
  type(type), len(len), data(data) {}

bool ArrayElem::operator < (const ArrayElem& other) const
{
  assert (this->type == other.type);
  assert (this->len == other.len);

  switch (this->type) {
  case INT: return *((int *)this->data) < *((int *)other.data);
  case FLOAT: return *((float *)this->data) < *((float *)other.data);
  case STRING: return strncmp (this->data, other.data, this->len) < 0;
  case PAGE_NUMBER: return true; // We don't compare page numbers.
  }
}

bool ArrayElem::operator > (const ArrayElem& other) const
{
  assert (this->type == other.type);
  assert (this->len == other.len);

  switch (this->type) {
  case INT: return *((int *)this->data) > *((int *)other.data);
  case FLOAT: return *((float *)this->data) > *((float *)other.data);
  case STRING: return strncmp (this->data, other.data, this->len) > 0;
  case PAGE_NUMBER: return true; // We don't compare page numbers.
  }
}

bool ArrayElem::operator == (const ArrayElem& other) const
{
  assert (this->type == other.type);
  assert (this->len == other.len);

  switch (this->type) {
  case INT: return *((int *)this->data) == *((int *)other.data);
  case FLOAT: return *((float *)this->data) == *((float *)other.data);
  case STRING: return strncmp (this->data, other.data, this->len) == 0;
  case PAGE_NUMBER: return true; // We don't compare page numbers.
  }
}

bool ArrayElem::operator != (const ArrayElem& other) const
{
  return !(*this == other);
}

bool ArrayElem::operator <= (const ArrayElem& other) const
{
  return !(*this > other);
}

bool ArrayElem::operator >= (const ArrayElem& other) const
{
  return !(*this < other);
}

Array::Array (char* data, int elem_size, int num_elems, AttrType elem_type)
  :data (data),
   elem_size (elem_size),
   len (num_elems),
   elem_type (elem_type) {}

const ArrayElem Array::operator [] (int i) const
{
  assert (i < this->len && i >= 0);
  
  ArrayElem elem (this->elem_type,
                  this->elem_size,
                  this->data + i * this->elem_size);
  return elem;
}

void Array::set (int i, const char* elem_ptr)
{
  assert (i < this->len && i >= 0);
  
  memcpy (this->data + i * this->elem_size,
          elem_ptr,
          this->elem_size);
}

void Array::insert (int i, const char* elem_ptr)
{
  assert (i+1 < this->len && i >= 0);

  // Move all the elements that have index >= i
  memmove (this->data + (i+1) * this->elem_size,
           this->data + i * this->elem_size,
           (this->len - i) * this->elem_size);
  this->len++;
  
  // set the ith element.
  this->set (i, elem_ptr);
}
