#pragma once

#include <vector>

#include "RM.h"
#include "IX.h"
#include "SM.h"

struct condition
{
  AttrType attr_type;
  int attr_len;
  CompOp comp_op;

  int offset1;
  bool has_rhs_attr;
  int offset2;

  void* value;

  bool satisfies (char* tuple) const;
  bool satisfies (char* tuple1, char* tuple2) const;
};

class Iterator
{
public:
  virtual ~Iterator () {}
  virtual void open () = 0;
  virtual char* next () = 0;
  virtual void close () = 0;
  virtual int tuple_size () const = 0;
  virtual void reset () = 0;
};

class RelIterator: public virtual Iterator
{
private:
  char* tuple_buffer;
  RM::Scan scan;
  RM::FileHandle rel;
  RM::Manager* rmm;
  const vector<condition> conditions;

public:
  RelIterator (const char* rel_name,
               const vector<condition>& conditions,
               RM::Manager* rmm,
               IX::Manager* ixm,
               SM::Manager* smm);
  ~RelIterator ();

  void open ();
  char* next ();
  void close ();
  int tuple_size () const;
  void reset ();
};

class CompositeIterator: public virtual Iterator
{
private:
  Iterator* iter1;
  Iterator* iter2;
  char* tuple_buffer;
  const vector<condition>& conditions;
  char* tuple1;

public:
  CompositeIterator (Iterator* iter1,
                     Iterator* iter2,
                     const vector<condition>& conditions);
  ~CompositeIterator ();

  void open ();
  char* next ();
  void close ();
  int tuple_size () const;
  void reset ();
};
