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
#include <dlfcn.h>
#include <cassert>
#include <unistd.h>
#include <string>
#include <map>
#include <set>
#include "redbase.h"
#include "ql.h"
#include "SM.h"
#include "IX.h"
#include "RM.h"
#include "printer.h"
#include "iterator.h"
#include "Blob.h"
#include "imagelib.h"

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
// Handle the selecteclnuse
//
RC QL_Manager::Select(int nSelAttrs, const RelAttr selAttrs[],
                      int nRelations, const char * const relations[],
                      int nConditions, Condition conditions[])
{
  int tuple_offsets [nRelations];
  tuple_offsets [0] = 0;
  map <string, vector <RM::Record> > table_attributes;
  set <string> source_relations;
  for (int i = 0; i < nRelations; ++i) {
    if (source_relations.find (relations [i]) != source_relations.end()) {
      cout << "Table " << relations [i] << " appears multiple times." << endl;
      return 184;
    }
    source_relations.insert (relations [i]);
  }
  for (int i = 1; i < nRelations; ++i) {
    Table* tbl = (Table *) this->smm->GetTableMetadata (relations [i-1]).data;
    table_attributes [relations [i-1]] =
      this->smm->GetAttributes (relations [i-1]);
    tuple_offsets [i] = tuple_offsets [i-1] + tbl->row_len;
  }
  table_attributes [relations [nRelations - 1]] =
    this->smm->GetAttributes (relations [nRelations - 1]);
  map <string, vector <string> > attr_to_table;
  map <string, int> tuple_offsets_map;
  for (int i = 0; i < nRelations; ++i) {
    tuple_offsets_map [relations [i]] = tuple_offsets [i];
    for (unsigned int j = 0;
         j < table_attributes [relations [i]].size();
         ++j) {
      char* a_name = ((Attribute *)table_attributes[relations[i]][j].data)->name;
      if (attr_to_table.find (a_name) == attr_to_table.end ()) {
        vector <string> table_list;
        attr_to_table [a_name] = table_list;
      }
      attr_to_table [a_name].push_back (relations [i]);
    }
  }

  vector <RelAttr> sel_attrs;
  if (nSelAttrs == 1 and selAttrs[0].attrName[0] == '*') {
    for (int i = 0; i < nRelations; ++i) {
      for (unsigned int j = 0;
           j < table_attributes [relations [i]].size ();
           ++j) {
        RelAttr r;
        r.attrName = ((Attribute *)table_attributes[relations[i]][j].data)->name;
        r.relName=((Attribute*)table_attributes[relations[i]][j].data)->table_name;
        sel_attrs.push_back (r);
      }
    }
  }
  else {
    for (int i = 0; i < nSelAttrs; ++i) {
      RelAttr r = selAttrs [i];
      if (r.relName == NULL)
        r.relName = attr_to_table [r.attrName][0].c_str();
      sel_attrs.push_back (r);
    }
  }
  
  map <string, map <string, Attribute> > attributes;
  for (int i = 0; i < nRelations; ++i) {
    map <string, Attribute> m;
    attributes [relations [i]] = m;
  }


  for (unsigned int i = 0; i < sel_attrs.size(); ++i) {
    if (source_relations.find (sel_attrs [i].relName) == source_relations.end ()) {
      cout << "Table " << sel_attrs [i].relName << " not in FROM clause." << endl;
      return 184;
    }
  }

  DataAttrInfo attrs [sel_attrs.size()];
  for (unsigned int i = 0; i < sel_attrs.size(); ++i) {
    Attribute* a = (Attribute*) this->smm->GetAttrMetadata (
      sel_attrs [i].relName,
      sel_attrs [i].attrName
    ).data;
    attributes [sel_attrs [i].relName] [sel_attrs [i].attrName] = *a;

    new (attrs + i) DataAttrInfo (*a);
    string tbl_name (attrs [i].relName);
    string attr_name (attrs [i].attrName);
    attrs [i].offset += tuple_offsets_map [tbl_name];
    strcpy (attrs [i].attrName, attr_name.c_str());
    if (attr_to_table [attr_name].size () != 1)
      strcpy (attrs [i].relName, tbl_name.c_str());
  }

  for (int i = 0; i < nConditions; ++i) {
    Condition* c = conditions + i;
    if (c->lhsAttr.relName == NULL) {
      c->lhsAttr.relName = attr_to_table [c->lhsAttr.attrName][0].c_str();
    }

    if (not c->bRhsIsAttr) continue;

    if (c->rhsAttr.relName == NULL) {
      c->rhsAttr.relName = attr_to_table [c->rhsAttr.attrName][0].c_str();
    }
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
          if (a.type == INT and c.rhsValue.type == STRING)
            *(int*)c.rhsValue.data = atoi((char*)c.rhsValue.data);
          else if (a.type == FLOAT and c.rhsValue.type == STRING)
            *(float*)c.rhsValue.data = atof((char*)c.rhsValue.data);
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
  Printer printer (attrs, sel_attrs.size());
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
  map <string, vector <RM::Record> > table_attributes;
  table_attributes [relName] = this->smm->GetAttributes (relName);

  map <string, vector <string> > attr_to_table;
  for (unsigned int j = 0;
       j < table_attributes [relName].size();
       ++j) {
    char* a_name = ((Attribute *)table_attributes[relName][j].data)->name;
    if (attr_to_table.find (a_name) == attr_to_table.end ()) {
      vector <string> table_list;
      attr_to_table [a_name] = table_list;
    }
    attr_to_table [a_name].push_back (relName);
  }

  vector <RelAttr> sel_attrs;
  for (unsigned int j = 0;
       j < table_attributes [relName].size ();
       ++j) {
    RelAttr r;
    r.attrName = ((Attribute *)table_attributes[relName][j].data)->name;
    r.relName=((Attribute*)table_attributes[relName][j].data)->table_name;
    sel_attrs.push_back (r);
  }
  
  map <string, map <string, Attribute> > attributes;
  map <string, Attribute> m;
  attributes [relName] = m;

  DataAttrInfo attrs [sel_attrs.size()];
  for (unsigned int i = 0; i < sel_attrs.size(); ++i) {
    Attribute* a = (Attribute*) this->smm->GetAttrMetadata (
      sel_attrs [i].relName,
      sel_attrs [i].attrName
    ).data;
    attributes [sel_attrs [i].relName] [sel_attrs [i].attrName] = *a;

    new (attrs + i) DataAttrInfo (*a);
    string tbl_name (attrs [i].relName);
    string attr_name (attrs [i].attrName);
    strcpy (attrs [i].attrName, attr_name.c_str());
    if (attr_to_table [attr_name].size () != 1)
      strcpy (attrs [i].relName, tbl_name.c_str());
  }

  for (int i = 0; i < nConditions; ++i) {
    Condition c = conditions [i];
    Attribute* a = (Attribute*) this->smm->GetAttrMetadata (
      relName,
      c.lhsAttr.attrName
    ).data;
    attributes [relName] [c.lhsAttr.attrName] = *a;

    if (not c.bRhsIsAttr) continue;
    a = (Attribute*) this->smm->GetAttrMetadata (
      relName,
      c.rhsAttr.attrName
    ).data;
    attributes [relName] [c.rhsAttr.attrName] = *a;
  }

  map <string, vector<condition> > single_table_conditions;
  vector<condition> conds;
  for (int cond_i = 0; cond_i < nConditions; ++cond_i) {
    Condition c = conditions [cond_i];
    if (not c.bRhsIsAttr) {
      condition cond;
      Attribute a = attributes [relName] [c.lhsAttr.attrName];

      cond.attr_type = a.type;
      cond.attr_len = a.len;
      cond.comp_op = c.op;
      cond.offset1 = a.offset;
      cond.has_rhs_attr = false;
      if (a.type == INT and c.rhsValue.type == STRING)
        *(int*)c.rhsValue.data = atoi((char*)c.rhsValue.data);
      else if (a.type == FLOAT and c.rhsValue.type == STRING)
        *(float*)c.rhsValue.data = atof((char*)c.rhsValue.data);
      cond.value = c.rhsValue.data;

      conds.push_back (cond);
    }
    else {
      condition cond;
      Attribute a1 = attributes [relName] [c.lhsAttr.attrName];
      Attribute a2 = attributes [relName] [c.rhsAttr.attrName];
            
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
  single_table_conditions [relName] = conds;

  auto iter = new RelIterator (relName,
                               single_table_conditions [relName],
                               this->rmm,
                               this->ixm,
                               this->smm);
  Printer printer (attrs, sel_attrs.size());
  printer.PrintHeader (cout);

  char* rec;
  iter->open();
  while ((rec = iter->next()) != NULL) {
    printer.Print (cout, rec);
    this->smm->Delete (relName, rec, iter->rid());
  }
  iter->close();

  printer.PrintFooter (cout);
  delete iter;
  return 0;
}

void textprint (Blob& blob)
{
  int len = blob.size();
  void* data = malloc (len);
  blob.read (data, len, 0);
  cout << (char*) data << endl;
  free (data);
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
  map <string, vector <RM::Record> > table_attributes;
  table_attributes [relName] = this->smm->GetAttributes (relName);
  int record_size = 0;

  map <string, vector <string> > attr_to_table;
  for (unsigned int j = 0;
       j < table_attributes [relName].size();
       ++j) {
    Attribute * a = (Attribute *) table_attributes [relName][j].data;
    record_size += a->len;
    if (attr_to_table.find (a->name) == attr_to_table.end ()) {
      vector <string> table_list;
      attr_to_table [a->name] = table_list;
    }
    attr_to_table [a->name].push_back (relName);
  }

  vector <RelAttr> sel_attrs;
  for (unsigned int j = 0;
       j < table_attributes [relName].size ();
       ++j) {
    RelAttr r;
    r.attrName = ((Attribute *)table_attributes[relName][j].data)->name;
    r.relName=((Attribute*)table_attributes[relName][j].data)->table_name;
    sel_attrs.push_back (r);
  }
  
  map <string, map <string, Attribute> > attributes;
  map <string, Attribute> m;
  attributes [relName] = m;

  DataAttrInfo attrs [sel_attrs.size()];
  for (unsigned int i = 0; i < sel_attrs.size(); ++i) {
    Attribute* a = (Attribute*) this->smm->GetAttrMetadata (
      sel_attrs [i].relName,
      sel_attrs [i].attrName
    ).data;
    attributes [sel_attrs [i].relName] [sel_attrs [i].attrName] = *a;

    new (attrs + i) DataAttrInfo (*a);
    string tbl_name (attrs [i].relName);
    string attr_name (attrs [i].attrName);
    strcpy (attrs [i].attrName, attr_name.c_str());
    if (attr_to_table [attr_name].size () != 1)
      strcpy (attrs [i].relName, tbl_name.c_str());
  }

  for (int i = 0; i < nConditions; ++i) {
    Condition c = conditions [i];
    Attribute* a = (Attribute*) this->smm->GetAttrMetadata (
      relName,
      c.lhsAttr.attrName
    ).data;
    attributes [relName] [c.lhsAttr.attrName] = *a;

    if (not c.bRhsIsAttr) continue;
    a = (Attribute*) this->smm->GetAttrMetadata (
      relName,
      c.rhsAttr.attrName
    ).data;
    attributes [relName] [c.rhsAttr.attrName] = *a;
  }

  map <string, vector<condition> > single_table_conditions;
  vector<condition> conds;
  for (int cond_i = 0; cond_i < nConditions; ++cond_i) {
    Condition c = conditions [cond_i];
    if (not c.bRhsIsAttr) {
      condition cond;
      Attribute a = attributes [relName] [c.lhsAttr.attrName];

      cond.attr_type = a.type;
      cond.attr_len = a.len;
      cond.comp_op = c.op;
      cond.offset1 = a.offset;
      cond.has_rhs_attr = false;
      if (a.type == INT and c.rhsValue.type == STRING)
        *(int*)c.rhsValue.data = atoi((char*)c.rhsValue.data);
      else if (a.type == FLOAT and c.rhsValue.type == STRING)
        *(float*)c.rhsValue.data = atof((char*)c.rhsValue.data);
      cond.value = c.rhsValue.data;

      conds.push_back (cond);
    }
    else {
      condition cond;
      Attribute a1 = attributes [relName] [c.lhsAttr.attrName];
      Attribute a2 = attributes [relName] [c.rhsAttr.attrName];
            
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
  single_table_conditions [relName] = conds;

  auto iter = new RelIterator (relName,
                               single_table_conditions [relName],
                               this->rmm,
                               this->ixm,
                               this->smm);
  Printer printer (attrs, sel_attrs.size());
  printer.PrintHeader (cout);

  Attribute* attr_to_update = (Attribute *) this->smm->GetAttrMetadata (
    relName,
    updAttr.attrName
  ).data;

  char* rec;
  vector <char*> recs_to_insert;

  iter->open();
  while ((rec = iter->next()) != NULL) {
    char* new_rec = new char [record_size];
    memcpy (new_rec, rec, record_size);
    if (bIsValue) {
      if (attr_to_update->type == BLOB) {
        // Figure out whether we are setting a new file or we are
        // calling a function on the blob.
        string val ((const char*)rhsValue.data);
        if (val.find ('.') == string::npos) {
          // Not a filename, this is an in-place update function.
          void (*updator) (Blob&);
          bool updated = false;
          updated = true;
          for (unsigned int i = 0; i < this->smm->libraries.size(); ++i) {
            cout << "trying to load " << val.c_str() << endl;
            dlerror();
            *(void **)(&updator) = dlsym (this->smm->libraries[i], val.c_str());
            char* error;
            if ((error = dlerror()) != NULL) continue;
            else {
              int* blob_id = (int*)(rec + attr_to_update->offset);
              Blob b = this->rmm->GetBlob (
                relName,
                *blob_id
              );
              cout << "calling updator" << endl;
              // textprint(b);
              (*updators["textprint"])(b);
              updated = true;
            }
          }
          if (not updated) {
            cout << "Function " << val << " not found." << endl;
            return 199;
          }
        }
        else {
          int blob_id = this->rmm->MakeBlob (relName, val.c_str());
          memcpy (new_rec + attr_to_update->offset,
                  &blob_id,
                  attr_to_update->len);
        }
      }
      else {
        memcpy (new_rec + attr_to_update->offset,
                rhsValue.data,
                attr_to_update->len);
      }
    }
    else {
      Attribute* attr_to_copy = (Attribute *) this->smm->GetAttrMetadata (
        relName,
        rhsRelAttr.attrName
      ).data;
      memcpy (new_rec + attr_to_update->offset,
              new_rec + attr_to_copy->offset,
              attr_to_update->len);
    }
    recs_to_insert.push_back (new_rec);

    printer.Print (cout, new_rec);
    this->smm->Delete (relName, rec, iter->rid());
  }
  iter->close();

  for (unsigned int i = 0; i < recs_to_insert.size (); ++i) {
    this->smm->Insert (relName, recs_to_insert [i]);
    delete [] recs_to_insert [i];
  }

  printer.PrintFooter (cout);
  delete iter;
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
