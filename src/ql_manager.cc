//
// ql_manager.cc
//

// Note that for the SM component (HW3) the QL is actually a
// simple stub that will allow everything to compile.  Without
// a QL stub, we would need two parsers.

#include <cstdio>
#include <iostream>
#include <sys/times.h>
#include <sys/types.h>
#include <cassert>
#include <unistd.h>
#include <string>
#include <map>
#include "redbase.h"
#include "ql.h"
#include "SM.h"
#include "IX.h"
#include "RM.h"
#include "printer.h"
#include "iterator.h"

using namespace std;

//
// QL_Manager::QL_Manager(SM_Manager &smm, IX_Manager &ixm, RM_Manager &rmm)
//
// Constructor for the QL Manager
//
QL_Manager::QL_Manager(SM::Manager &smm, IX::Manager &ixm, RM::Manager &rmm)
  : smm(&smm), ixm(&ixm), rmm(&rmm)
{
  // Can't stand unused variable warnings!
  assert (&smm && &ixm && &rmm);
}

//
// QL_Manager::~QL_Manager()
//
// Destructor for the QL Manager
//
QL_Manager::~QL_Manager()
{
}

//
// Handle the select clause
//
RC QL_Manager::Select(int nSelAttrs, const RelAttr selAttrs[],
                      int nRelations, const char * const relations[],
                      int nConditions, const Condition conditions[])
{
  // In the resulting big tuple, calculate the individual tuple offsets.
  int tuple_offsets [nRelations];
  tuple_offsets [0] = 0;
  for (int i = 1; i < nRelations; ++i) {
    Table* tbl = (Table *) this->smm->GetTableMetadata (relations [i-1]).data;
    tuple_offsets [i] = tuple_offsets [i-1] + tbl->row_len;
  }
  map <string, int> tuple_offsets_map;
  for (int i = 0; i < nRelations; ++i) {
    tuple_offsets_map [relations [i]] = tuple_offsets [i];
  }

  
  map <string, map <string, Attribute> > attributes;
  for (int i = 0; i < nRelations; ++i) {
    map <string, Attribute> m;
    attributes [relations [i]] = m;
  }

  DataAttrInfo attrs [nSelAttrs];
  for (int i = 0; i < nSelAttrs; ++i) {
    Attribute* a = (Attribute*) this->smm->GetAttrMetadata (
      selAttrs [i].relName,
      selAttrs [i].attrName
    ).data;
    attributes [selAttrs [i].relName] [selAttrs [i].attrName] = *a;

    new (attrs + i) DataAttrInfo (*a);
    string tbl_name (attrs [i].relName);
    string attr_name (attrs [i].attrName);
    attrs [i].offset += tuple_offsets_map [tbl_name];
    strcpy (attrs [i].attrName, (tbl_name + '.' + attr_name).c_str());
  }

  for (int i = 0; i < nConditions; ++i) {
    Condition c = conditions [i];
    Attribute* a = (Attribute*) this->smm->GetAttrMetadata (
      c.lhsAttr.relName,
      c.lhsAttr.attrName
    ).data;
    attributes [c.lhsAttr.relName] [c.lhsAttr.attrName] = *a;

    if (not c.bRhsIsAttr) continue;
    a = (Attribute*) this->smm->GetAttrMetadata (
      c.rhsAttr.relName,
      c.rhsAttr.attrName
    ).data;
    attributes [c.rhsAttr.relName] [c.rhsAttr.attrName] = *a;
  }

  map <string, vector<condition> > cross_table_conditions;
  for (int i = 1; i < nRelations; ++i) {
    vector <condition> conds;
    for (int cond_i = 0; cond_i < nConditions; ++cond_i) {
      Condition c = conditions [cond_i];
      if (not c.bRhsIsAttr) continue;
      if (strcmp (relations [i], c.lhsAttr.relName) == 0) {
        RelAttr l = c.lhsAttr;
        RelAttr r = c.rhsAttr;
        c.lhsAttr = r;
        c.rhsAttr = l;
      }
      if (strcmp (relations [i], c.rhsAttr.relName) != 0) continue;
      bool other_attr_from_seen_tables = false;
      for (int j = 0; j < i; ++j) {
        if (strcmp (relations [j], c.lhsAttr.relName) == 0) {
          other_attr_from_seen_tables = true;
          break;
        }
      }
      if (not other_attr_from_seen_tables) continue;
      condition cond;
      Attribute a1 = attributes [c.lhsAttr.relName] [c.lhsAttr.attrName];
      Attribute a2 = attributes [c.rhsAttr.relName] [c.rhsAttr.attrName];

      assert (a1.type == a2.type);
      assert (a1.len == a2.len);

      cond.attr_type = a1.type;
      cond.attr_len = a1.len;
      cond.comp_op = c.op;
      cond.offset1 = a1.offset + tuple_offsets_map [a1.table_name];
      cond.offset2 = a2.offset;
      cond.has_rhs_attr = true;

      conds.push_back (cond);
    }
    cross_table_conditions [relations [i]] = conds;
  }

  map <string, vector<condition> > single_table_conditions;
  for (int i = 0; i < nRelations; ++i) {
    vector<condition> conds;
    for (int cond_i = 0; cond_i < nConditions; ++cond_i) {
      Condition c = conditions [cond_i];
      if (strcmp (relations [i], c.lhsAttr.relName) == 0) {
        if (not c.bRhsIsAttr) {
          condition cond;
          Attribute a = attributes [c.lhsAttr.relName] [c.lhsAttr.attrName];

          cond.attr_type = a.type;
          cond.attr_len = a.len;
          cond.comp_op = c.op;
          cond.offset1 = a.offset;
          cond.has_rhs_attr = false;
          cond.value = c.rhsValue.data;

          conds.push_back (cond);
        }
        else {
          if (strcmp (relations [i], c.rhsAttr.relName) == 0) {
            condition cond;
            Attribute a1 = attributes [c.lhsAttr.relName] [c.lhsAttr.attrName];
            Attribute a2 = attributes [c.rhsAttr.relName] [c.rhsAttr.attrName];
            
            assert (a1.type == a2.type);
            assert (a1.len == a2.len);

            cond.attr_type = a1.type;
            cond.attr_len = a1.len;
            cond.comp_op = c.op;
            cond.offset1 = a1.offset;
            cond.offset2 = a2.offset;
            cond.has_rhs_attr = true;

            conds.push_back (cond);
          }
        }
      }
    }
    single_table_conditions [relations [i]] = conds;
  }

  Iterator* iter = new RelIterator (relations[0],
                                    single_table_conditions [relations [0]],
                                    this->rmm,
                                    this->ixm,
                                    this->smm);
  for (int i = 1; i < nRelations; ++i) {
    iter = new CompositeIterator (
      iter,
      new RelIterator (relations [i],
                       single_table_conditions [relations [i]],
                       this->rmm,
                       this->ixm,
                       this->smm),
      cross_table_conditions [relations [i]]
    );
  }
  Printer printer (attrs, nSelAttrs);
  printer.PrintHeader (cout);

  char* rec;
  iter->open();
  while ((rec = iter->next()) != NULL) {
    printer.Print (cout, rec);
  }
  iter->close();

  printer.PrintFooter (cout);
  delete iter;
  return 0;
}

//
// Insert the values into relName
//
RC QL_Manager::Insert(const char *relName,
                      int nValues, const Value values[])
{
  void* vals [nValues];
  for (int i = 0; i < nValues; ++i) {
    vals [i] = values [i].data;
  }
  this->smm->Insert (relName, vals);

  return 0;
}

//
// Delete from the relName all tuples that satisfy conditions
//
RC QL_Manager::Delete(const char *relName,
                      int nConditions, const Condition conditions[])
{
  int i;

  cout << "Delete\n";

  cout << "   relName = " << relName << "\n";
  cout << "   nCondtions = " << nConditions << "\n";
  for (i = 0; i < nConditions; i++)
    cout << "   conditions[" << i << "]:" << conditions[i] << "\n";

  return 0;
}


//
// Update from the relName all tuples that satisfy conditions
//
RC QL_Manager::Update(const char *relName,
                      const RelAttr &updAttr,
                      const int bIsValue,
                      const RelAttr &rhsRelAttr,
                      const Value &rhsValue,
                      int nConditions, const Condition conditions[])
{
  int i;

  cout << "Update\n";

  cout << "   relName = " << relName << "\n";
  cout << "   updAttr:" << updAttr << "\n";
  if (bIsValue)
    cout << "   rhs is value: " << rhsValue << "\n";
  else
    cout << "   rhs is attribute: " << rhsRelAttr << "\n";

  cout << "   nCondtions = " << nConditions << "\n";
  for (i = 0; i < nConditions; i++)
    cout << "   conditions[" << i << "]:" << conditions[i] << "\n";

  return 0;
}

//
// void QL_PrintError(RC rc)
//
// This function will accept an Error code and output the appropriate
// error.
//
void QL_PrintError(RC rc)
{
  cout << "QL_PrintError\n   rc=" << rc << "\n";
}
