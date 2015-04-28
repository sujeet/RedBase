#include <cassert>
#include <cstring>

#include "Array.h"

ArrayElem::ArrayElem ()
  : type(NONE), len(-1), data(NULL), needs_free(false) {}

ArrayElem::ArrayElem (AttrType type, int len, char* data)
  : type(type), len(len), data(data), needs_free(false) {}

ArrayElem::~ArrayElem ()
{
  if (this->needs_free) delete[] this->data;
}

ArrayElem::operator int () const
{
  assert (this->type == INT);
  assert (this->len == 4);

  return *(int*)this->data;
}

ArrayElem& ArrayElem::operator = (const ArrayElem& other)
{
  this->type = other.type;
  this->len = other.len;
  if (this->data == NULL) {
    this->needs_free = true;
    this->data = new char [this->len];
  }
  memcpy (this->data, other.data, this->len);
  return (*this);
}

ArrayElem& ArrayElem::operator = (int val)
{
  this->type = INT;
  this->len = 4;
  if (this->data == NULL) {
    this->needs_free = true;
    this->data = new char [this->len];
  }
  *(int*)(this->data) = val;
  return (*this);
}

bool ArrayElem::operator < (const ArrayElem& other) const
{
  assert (this->type != NONE);
  assert (this->type == other.type);
  assert (this->len == other.len);
  
  switch (this->type) {
  case INT: return *((int *)this->data) < *((int *)other.data);
  case FLOAT: return *((float *)this->data) < *((float *)other.data);
  case STRING: return strncmp (this->data, other.data, this->len) < 0;
  default: return false;
  }
}

bool ArrayElem::operator > (const ArrayElem& other) const
{
  assert (this->type != NONE);
  assert (this->type == other.type);
  assert (this->len == other.len);

  switch (this->type) {
  case INT: return *((int *)this->data) > *((int *)other.data);
  case FLOAT: return *((float *)this->data) > *((float *)other.data);
  case STRING: return strncmp (this->data, other.data, this->len) > 0;
  default: return false;
  }
}

bool ArrayElem::operator == (const ArrayElem& other) const
{
  assert (this->type != NONE);
  assert (this->type == other.type);
  assert (this->len == other.len);

  switch (this->type) {
  case INT: return *((int *)this->data) == *((int *)other.data);
  case FLOAT: return *((float *)this->data) == *((float *)other.data);
  case STRING: return strncmp (this->data, other.data, this->len) == 0;
  default: return false;
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

Array::Array (char* data,
              int elem_size,
              int num_elems,
              AttrType elem_type,
              int max_len)
  :data (data),
   elem_size (elem_size),
   len_ (num_elems),
   elem_type (elem_type),
   max_len (max_len) {}

void Array::init (char* data,
                  int elem_size,
                  int num_elems,
                  AttrType elem_type,
                  int max_len)
{
  this->data = data;
  this->elem_size = elem_size;
  this->len_ = num_elems;
  this->elem_type = elem_type;
  this->max_len = max_len;
}

int Array::len () const
{
  return this->len_;
}

using namespace std;

ArrayElem Array::operator [] (int i) const
{
  assert (i < this->len() && i >= 0);
  
  ArrayElem elem (this->elem_type,
                  this->elem_size,
                  this->data + i * this->elem_size);
  return elem;
}

ArrayElem Array::pop ()
{
  ArrayElem ret = (*this) [this->len() - 1];
  this->len_--;
  return ret;
}

void Array::move_from_i (int i)
{
  assert (i <= this->len() &&
          this->len() < this->max_len &&
          i >= 0);

  // Move all the elements that have index >= i
  memmove (this->data + (i+1) * this->elem_size,
           this->data + i * this->elem_size,
           (this->len() - i) * this->elem_size);
  this->len_++;
} 

void Array::insert (int i, const ArrayElem& elem)
{
  this->move_from_i (i);
  (*this) [i] = elem;
}

void Array::insert (int i, int val)
{
  this->move_from_i (i);
  (*this) [i] = val;
}
