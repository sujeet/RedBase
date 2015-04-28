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

  // Who allocated memory for the data?
  // if we did, then we need to free it too.
  bool needs_free;

public:
  ArrayElem (AttrType type, int len, char* data);
  ArrayElem ();
  ~ArrayElem ();

  ArrayElem& operator = (const ArrayElem& other);
  ArrayElem& operator = (int pg_num);

  operator int () const;

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
  char *data;
  int elem_size;
  int len_;
  AttrType elem_type;
  int max_len;
  void move_from_i (int i);

public:
  Array () {};
  Array (char* data,
         int elem_size,
         int num_elems,
         AttrType elem_type,
         int max_len);
  void init (char *data,
             int elem_size,
             int num_elems,
             AttrType elem_type,
             int max_len);

  int len () const;

  ArrayElem operator [] (int i) const;

  ArrayElem pop ();

  // now, the new element is at index i,
  // all the elements which had index >= i previously,
  // now have their indices increased by one.
  void insert (int i, const ArrayElem& elem);
  void insert (int i, int val);
};

#endif  // __ARRAY_H
