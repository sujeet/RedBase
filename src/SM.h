//
// sm.h
//   Data Manager Component Interface
//

#ifndef SM_H
#define SM_H

// Please do not include any other files than the ones below in this file.

#include <stdlib.h>
#include <string.h>
#include <vector>
#include "redbase.h"  // Please don't change these lines
#include "parser.h"
#include "RM.h"
#include "IX.h"

#define DB_NAME_MAXLEN 255

class warning: public exception {};

#pragma pack(push, 1)
struct Table
{
  char name [MAXNAME + 1];
  int row_len;
  int attr_count;
  int index_count;
  int next_index_num;
  Table (const char* name,
         int row_len,
         int attr_count,
         int index_count,
         int next_index_num);
};

struct Attribute
{
  char table_name [MAXNAME + 1];
  char name [MAXNAME + 1];
  int offset;
  AttrType type;
  int len;
  int index_num;
  Attribute () {}
  Attribute (const char* table_name,
             const char* name,
             int offset,
             AttrType type,
             int len,
             int index_num);
};
#pragma pack(pop)

namespace SM
{
class Manager
{
  friend class QL_Manager;

private:
  IX::Manager ixm;
  RM::Manager rmm;
  RM::FileHandle relcat;
  RM::FileHandle attrcat;

public:
  Manager    (IX::Manager &ixm, RM::Manager &rmm);
  ~Manager   ();                             // Destructor

  void OpenDb     (const char *dbName);           // Open the database
  void CloseDb    ();                             // close the database

  void CreateTable(const char *relName,           // create relation relName
                   int        attrCount,          //   number of attributes
                   AttrInfo   *attributes);       //   attribute data
  void CreateIndex(const char *relName,           // create an index for
                   const char *attrName);         //   relName.attrName
  void DropTable  (const char *relName);          // destroy a relation

  void DropIndex  (const char *relName,           // destroy index on
                   const char *attrName);         //   relName.attrName
  void Insert     (const char* relName,
                   void* values[]);
  void Insert     (const char* relName,
                   const char* rec_data);
  void Delete     (const char* relName,
                   char* rec_data,
                   RID rid);
  void Load       (const char *relName,           // load relName from
                   const char *fileName);         //   fileName
  void Help       ();                             // Print relations in db
  void Help       (const char *relName);          // print schema of relName

  void Print      (const char *relName);          // print relName contents

  void Set        (const char *paramName,         // set parameter to
                   const char *value);            //   value

  RM::Record GetTableMetadata (const char* relName) const;
  RM::Record GetAttrMetadata (const char* relName,
                              const char* attrName) const;
  vector<RM::Record> GetAttributes (const char* relName) const;
};

#define DECLARE_EXCEPTION(name, message)   \
class name: public exception               \
{                                          \
  virtual const char* what() const throw() \
  {                                        \
    return message;                        \
  }                                        \
};

#define DECLARE_WARNING(name, message)     \
class name: public warning                 \
{                                          \
  virtual const char* what() const throw() \
  {                                        \
    return message;                        \
  }                                        \
};

namespace error
{
DECLARE_EXCEPTION (BadArguments, "Bad arguments.");
DECLARE_EXCEPTION (DBNameTooLong, "DB name too long.");
DECLARE_EXCEPTION (ChdirError, "Could not change directory.");
DECLARE_EXCEPTION (UnknownAttributeType, "Unknown attribute type.");
}  // namespace error

namespace warn
{
DECLARE_WARNING (DuplicateAttributes, "Duplicate attributes.");
DECLARE_WARNING (TableAlreadyExists, "Table already exists.");
DECLARE_WARNING (TableDoesNotExist, "Table doesn't exist.");
DECLARE_WARNING (AttrDoesNotExist, "Attribute doesn't exist.");
DECLARE_WARNING (IndexAlreadyExists, "Index already exists.");
DECLARE_WARNING (IndexDoesNotExist, "Index desn't exist.");
DECLARE_WARNING (BadCSVFile, "Error opening CSV file.");
}  // namespace warning

}  // namespace SM

#endif
