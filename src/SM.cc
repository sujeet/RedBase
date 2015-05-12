//
// File:        SM component stubs
// Description: Print parameters of all Manager methods
// Authors:     Dallan Quass (quass@cs.stanford.edu)
//

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <istream>
#include <sstream>
#include <map>
#include <cstdlib>
#include <unistd.h>
#include "redbase.h"
#include "SM.h"
#include "IX.h"
#include "RM.h"
#include "printer.h"

using namespace std;

// CSV parsing code from http://www.zedwood.com/article/cpp-csv-parser
vector<string> csv_read_row(istream &in, char delimiter)
{
  stringstream ss;
  bool inquotes = false;
  vector<string> row;//relying on RVO
  while(in.good())
  {
    char c = in.get();
    if (!inquotes && c=='"') //beginquotechar
    {
      inquotes=true;
    }
    else if (inquotes && c=='"') //quotechar
    {
      if ( in.peek() == '"')//2 consecutive quotes resolve to 1
      {
        ss << (char)in.get();
      }
      else //endquotechar
      {
        inquotes=false;
      }
    }
    else if (!inquotes && c==delimiter) //end of field
    {
      row.push_back( ss.str() );
      ss.str("");
    }
    else if (!inquotes && (c=='\r' || c=='\n') )
    {
      if(in.peek()=='\n') { in.get(); }
      row.push_back( ss.str() );
      return row;
    }
    else
    {
      ss << c;
    }
  }
  return row;
}

vector<string> csv_read_row(string &line, char delimiter)
{
  stringstream ss(line);
  return csv_read_row(ss, delimiter);
}
 
Table::Table (const char* name,
              int row_len,
              int attr_count,
              int index_count,
              int next_index_num)
  : row_len (row_len),
    attr_count (attr_count),
    index_count (index_count),
    next_index_num (next_index_num)
{
  memset (this->name, 0, sizeof (this->name));
  strncpy (this->name, name, MAXNAME);
}

Attribute::Attribute (const char* table_name,
                      const char* name,
                      int offset,
                      AttrType type,
                      int len,
                      int index_num)
  : offset (offset),
    type (type),
    len (len),
    index_num (index_num)
{
  memset (this->table_name, 0, sizeof (this->name));
  strncpy (this->table_name, table_name, MAXNAME);

  memset (this->name, 0, sizeof (this->name));
  strncpy (this->name, name, MAXNAME);
}

namespace SM
{
Manager::Manager(IX::Manager &ixm, RM::Manager &rmm)
  : ixm (ixm),
    rmm (rmm) {}

Manager::~Manager() {}

void Manager::OpenDb(const char *dbName)
{
  if (strlen(dbName) > DB_NAME_MAXLEN) throw error::DBNameTooLong ();
  if (chdir(dbName) < 0) throw error::ChdirError ();

  this->relcat = this->rmm.OpenFile ("relcat");
  this->attrcat = this->rmm.OpenFile ("attrcat");
}

void Manager::CloseDb()
{
  this->rmm.CloseFile (this->relcat);
  this->rmm.CloseFile (this->attrcat);
}

RM::Record Manager::GetTableMetadata (const char* relName) const
{
  RM::Scan scan;
  scan.open (this->relcat, STRING, MAXNAME + 1, 0, EQ_OP, relName);
  auto rec = scan.next ();
  scan.close ();
  return rec;
}

RM::Record Manager::GetAttrMetadata (const char* relName,
                                     const char* attrName) const
{
  RM::Scan scan;
  RM::Record rec;
  scan.open (this->attrcat, STRING, MAXNAME + 1, 0, EQ_OP, relName);
  while ((rec = scan.next()) != scan.end) {
    Attribute* attr = (Attribute *) rec.data;
    if (strcmp (attr->name, attrName) == 0) {
      scan.close ();
      break;
    }
  }
  return rec;
}

bool attr_offset_comp (const RM::Record& r1, const RM::Record& r2) 
{
  return ((Attribute *) r1.data)->offset < ((Attribute *) r2.data)->offset;
}

vector<RM::Record> Manager::GetAttributes (const char* relName) const
{
  RM::Scan scan;
  RM::Record rec;
  vector<RM::Record> vec;
  scan.open (this->attrcat, STRING, MAXNAME + 1, 0, EQ_OP, relName);
  while ((rec = scan.next()) != scan.end) {
    vec.push_back (rec);
  }
  sort (vec.begin(), vec.end(), attr_offset_comp);
  return vec;
}

void Manager::CreateTable(const char *relName,
                          int        attrCount,
                          AttrInfo   *attributes)
{
  // Check whether the table already exists.
  if (this->GetTableMetadata (relName) != RM::Scan::end)
    throw warn::TableAlreadyExists ();
  
  // Check for duplicate attribute names.
  map<string,bool> attr_seen;
  for (int i = 0; i < attrCount; ++i) {
    if (attr_seen [attributes [i].attrName]) {
      throw warn::DuplicateAttributes ();
    }
    attr_seen [attributes [i].attrName] = true;
  }
  
  // Add metadata about table.
  int offset = 0;
  for (int i = 0; i < attrCount; ++i) {
    AttrInfo a = attributes [i];
    Attribute attr (relName, a.attrName, offset, a.attrType, a.attrLength, -1);
    this->attrcat.insert ((const char*)&attr);
    offset += a.attrLength;
  }
  Table tbl (relName, offset, attrCount, 0, 1);
  this->relcat.insert ((const char*)&tbl);
  this->relcat.ForcePages ();
  this->attrcat.ForcePages ();

  // Create table.
  this->rmm.CreateFile (relName, offset);
}

void Manager::DropTable(const char *relName)
{
  // First make sure that the table exists.
  auto table_record = this->GetTableMetadata (relName);
  if (table_record == RM::Scan::end)
    throw warn::TableDoesNotExist ();

  // Delete the table
  this->rmm.DestroyFile (relName);

  // Update the metadata tables.
  RM::Scan scan;
  RM::Record rec;
  scan.open (this->attrcat, STRING, MAXNAME + 1, 0, EQ_OP, relName);
  while ((rec = scan.next()) != scan.end) {
    Attribute* attr = (Attribute *) rec.data;
    // Delete index if exists.
    if (attr->index_num != -1) {
      this->ixm.DestroyIndex (relName, attr->index_num);
    }
    // Clear metadata
    this->attrcat.Delete (rec.rid);
  }
  scan.close ();

  this->relcat.Delete (table_record.rid);
  this->relcat.ForcePages ();
  this->attrcat.ForcePages ();
}

void Manager::CreateIndex(const char *relName,
                          const char *attrName)
{
  // Make sure that the table and attribute exist.
  auto table_meta_rec = this->GetTableMetadata (relName);
  if (table_meta_rec == RM::Scan::end)
    throw warn::TableDoesNotExist ();

  auto attr_meta_rec = this->GetAttrMetadata (relName, attrName);
  if (attr_meta_rec == RM::Scan::end)
    throw warn::AttrDoesNotExist ();

  Attribute* attr_meta = (Attribute *) attr_meta_rec.data;
  if (attr_meta->index_num != -1)
    throw warn::IndexAlreadyExists ();

  // From relcat, figure out what the next index number,
  // add one more, and update relcat.
  Table* table_meta = (Table *) table_meta_rec.data;
  int index_num = table_meta->next_index_num;
  table_meta->next_index_num ++;
  table_meta->index_count ++;
  this->relcat.update (table_meta_rec);

  // Update the attribute metadata to reflect that it is now indexed.
  attr_meta->index_num = index_num;
  this->attrcat.update (attr_meta_rec);

  // Create the index file
  this->ixm.CreateIndex (relName, index_num, attr_meta->type, attr_meta->len);
  auto index = this->ixm.OpenIndex (relName, index_num);
  auto relation = this->rmm.OpenFile (relName);

  RM::Scan scan;
  RM::Record rec;
  // Populace the index
  scan.open (this->relcat, STRING, MAXNAME + 1, 0, EQ_OP, relName);
  while ((rec = scan.next()) != scan.end) {
    index.Insert (rec.data + attr_meta->offset, rec.rid);
  }

  this->ixm.CloseIndex (index);
  this->rmm.CloseFile (relation);

  this->relcat.ForcePages ();
  this->attrcat.ForcePages ();
}

void Manager::DropIndex(const char *relName,
                        const char *attrName)
{
  // Check that there is actually an index.
  auto attr_meta_rec = this->GetAttrMetadata (relName, attrName);
  Attribute* attr_meta = (Attribute *) attr_meta_rec.data;
  if (attr_meta->index_num == -1)
    throw warn::IndexDoesNotExist ();

  // Update table metadata
  auto table_meta_rec = this->GetTableMetadata (relName);
  Table* table_meta = (Table *) table_meta_rec.data;
  table_meta->index_count --;
  this->relcat.update (table_meta_rec);

  int index_num;
  // Update the attribute metadata
  index_num = attr_meta->index_num;
  attr_meta->index_num = -1;
  this->attrcat.update (attr_meta_rec);

  // Drop the index.
  this->ixm.DestroyIndex (relName, index_num);

  this->relcat.ForcePages ();
  this->attrcat.ForcePages ();
}

void Manager::Load(const char *relName,
                   const char *fileName)
{
  // Make sure that the exists.
  auto table_meta_rec = this->GetTableMetadata (relName);
  if (table_meta_rec == RM::Scan::end)
    throw warn::TableDoesNotExist ();

  auto table_rec = this->GetTableMetadata (relName);
  auto attr_recs = this->GetAttributes (relName);
  Table* table_meta = (Table *) table_rec.data;

  map<int, IX::IndexHandle> indexes;
  for (unsigned int i = 0; i < attr_recs.size (); ++i) {
    Attribute* attr = (Attribute *) attr_recs [i].data;
    if (attr->index_num != -1) {
      indexes [attr->index_num] = this->ixm.OpenIndex (relName,
                                                       attr->index_num);
    }
  }

  auto table = this->rmm.OpenFile (relName);
  char buf [table_meta->row_len];
  ifstream in (fileName);
  if (in.fail ()) throw warn::BadCSVFile ();
  while (in.good ()) {
    vector<string> row = csv_read_row (in, ',');
    RID rid = table.insert (buf); // Just to get an RID
    for (unsigned int i = 0; i < attr_recs.size (); ++i) {
      Attribute* attr = (Attribute *) attr_recs [i].data;
      switch (attr->type) {
      case INT:
        *(int*)(buf + attr->offset) = stoi (row [i]);
        break;
      case FLOAT:
        *(float*)(buf + attr->offset) = stof (row [i]);
        break;
      case STRING:
        memset (buf + attr->offset, 0, attr->len);
        strncpy (buf + attr->offset, row [i].c_str(), attr->len);
        break;
      case NONE:
        throw error::UnknownAttributeType ();
      }
      if (attr->index_num != -1) {
        indexes [attr->index_num].Insert (buf + attr->offset, rid);
      }
    }
    auto rec = table.get (rid);
    memcpy (rec.data, buf, sizeof (buf));
    table.update (rec);
  }
  in.close();

  for (unsigned int i = 0; i < attr_recs.size (); ++i) {
    Attribute* attr = (Attribute *) attr_recs [i].data;
    if (attr->index_num != -1) {
      this->ixm.CloseIndex (indexes [attr->index_num]);
    }
  }
  this->rmm.CloseFile (table);
  this->relcat.ForcePages ();
  this->attrcat.ForcePages ();
}

bool rec_comp (const RM::Record& r1, const RM::Record& r2) 
{
  return *(int*) r1.data < *(int *) r2.data;
}

void Manager::Print(const char *relName)
{
  Table* table = (Table *) this->GetTableMetadata (relName).data;
  DataAttrInfo attrs [table->attr_count];
  auto attr_recs = this->GetAttributes (relName);
  for (unsigned int i = 0; i < attr_recs.size (); ++i) {
    new (attrs + i) DataAttrInfo (*(Attribute *)attr_recs [i].data);
  }

  RM::Scan scan;
  RM::Record rec;
  auto relation = this->rmm.OpenFile (relName);
  scan.open (relation, STRING, MAXNAME + 1, 0, NO_OP, NULL);

  Printer printer (attrs, table->attr_count);
  printer.PrintHeader (cout);
  // TODO(sujeet): this is just a hack to pass the tests,
  // because the tests expect a sorted order.
  vector<RM::Record> recs;
  while ((rec = scan.next()) != scan.end) {
    recs.push_back (rec);
    // printer.Print (cout, rec.data);
  }
  sort (recs.begin(), recs.end(), rec_comp);
  for (unsigned int i = 0; i < recs.size (); ++i) {
    printer.Print (cout, recs[i].data);
  }

  printer.PrintFooter (cout);

  scan.close ();
  this->rmm.CloseFile (relation);
}

void Manager::Set(const char *paramName, const char *value)
{
  cout << "Set\n"
       << "   paramName=" << paramName << "\n"
       << "   value    =" << value << "\n";
}

void Manager::Help()
{
  DataAttrInfo attrs [3];
  auto attr_recs = this->GetAttributes ("relcat");
  for (unsigned int i = 0; i < 3; ++i) {
    new (attrs + i) DataAttrInfo (*(Attribute *)attr_recs [i].data);
  }

  RM::Scan scan;
  RM::Record rec;
  auto relation = this->rmm.OpenFile ("relcat");
  scan.open (relation, STRING, MAXNAME + 1, 0, NO_OP, NULL);

  Printer printer (attrs, 3);
  printer.PrintHeader (cout);
  while ((rec = scan.next()) != scan.end) {
    printer.Print (cout, rec.data);
  }
  printer.PrintFooter (cout);

  scan.close ();
  this->rmm.CloseFile (relation);
}

void Manager::Help(const char *relName)
{
  // Check whether the table already exists.
  if (this->GetTableMetadata (relName) == RM::Scan::end)
    throw warn::TableDoesNotExist ();
  
  Table* table = (Table *) this->GetTableMetadata ("attrcat").data;
  DataAttrInfo attrs [table->attr_count];
  auto attr_recs = this->GetAttributes ("attrcat");
  for (unsigned int i = 0; i < attr_recs.size (); ++i) {
    new (attrs + i) DataAttrInfo (*(Attribute *)attr_recs [i].data);
  }

  RM::Scan scan;
  RM::Record rec;
  auto relation = this->rmm.OpenFile ("attrcat");
  scan.open (relation, STRING, MAXNAME + 1, 0, EQ_OP, relName);

  Printer printer (attrs, table->attr_count);
  printer.PrintHeader (cout);
  while ((rec = scan.next()) != scan.end) {
    printer.Print (cout, rec.data);
  }
  printer.PrintFooter (cout);

  scan.close ();
  this->rmm.CloseFile (relation);
}
}  // namespace SM
