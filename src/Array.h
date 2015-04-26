#ifndef __ARRAY_H
#define __ARRAY_H

#include "redbase.h"

class Array;
class ArrayElem;


// ArrayElem is a wrapper class around
// the kind of things whose arrays we would
// like to store on a page, and easily compare
// them using the usual comparison operators.
class ArrayElem
{
  friend class Array;

private:
  // Type of the element.
  // can be an int, float, string or page number
  AttrType type;

  // Length of a single element in bytes.
  int len;

  // Pointer to the actual element.
  char* data;

  // No one except an array should be able to create
  // an array element.
  ArrayElem (AttrType type, int len, char* data);

public:
  bool operator < (const ArrayElem& other) const;
  bool operator > (const ArrayElem& other) const;
  bool operator == (const ArrayElem& other) const;
  bool operator != (const ArrayElem& other) const;
  bool operator <= (const ArrayElem& other) const;
  bool operator >= (const ArrayElem& other) const;
};


class Array
{
private:
  int len;
  char *data;
  AttrType elem_type;
  int elem_size;

public:
  Array (char* data, int elem_size, int num_elems, AttrType elem_type);
  ~Array () {};

  // Note that this is only a convenient getter,
  // as setting is done occassionally, if ever.
  const ArrayElem operator [] (int i) const;

  void set (int i, const char* elem_ptr);

  // now, the new element is at index i,
  // all the elements which had index >= i previously,
  // now have their indices increased by one.
  void insert (int i, const char *elem_ptr);
};

#endif  // __ARRAY_H
