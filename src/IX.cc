#include "IX.h"

#include <cassert>
#include <cstring>
#include <string>
#include <new>

using namespace std;

namespace IX
{

Manager::Manager (PF::Manager &pfm) : pfm (pfm) {};
Manager::Manager (PF_Manager &pfm) : pfm (pfm) {};

const string make_index_name (const char* fileName, int indexNo)
{
  string index_name (fileName);
  index_name += "." + std::to_string (indexNo);
  return index_name;
}

void Manager::CreateIndex (const char* fileName,
                           int indexNo,
                           AttrType attrType,
                           int attrLength)
{
  if ((indexNo < 0) ||
      ((attrType == INT) && (attrLength != 4)) ||
      ((attrType == FLOAT) && (attrLength != 4)) ||
      ((attrType == STRING) && (attrLength < 1 || attrLength > MAXSTRINGLEN)))
    throw error::BadArguments ();

  if (attrType != INT &&
      attrType != FLOAT &&
      attrType != STRING)
    throw error::BadArguments ();
  
  string index_name = make_index_name (fileName, indexNo);

  // Create an empty file and open it.
  this->pfm.CreateFile (index_name);
  PF::FileHandle index_file = this->pfm.OpenFile (index_name);

  // Add the root page of the index to the empty file.
  PF::PageHandle root_page = index_file.AllocatePage ();
  TreePageHdr hdr;
  hdr.num_keys = 0;
  hdr.key_size = attrLength;
  hdr.key_type = attrType;
  hdr.is_root = true;
  hdr.is_leaf = true;
  TreePage root (root_page.GetData (), hdr);

  // cleanup
  index_file.DoneWritingTo (root_page);
  this->pfm.CloseFile (index_file);
}

void Manager::DestroyIndex (const char *fileName, int indexNo)
{
  this->pfm.DestroyFile (make_index_name (fileName, indexNo));
}

IndexHandle Manager::OpenIndex (const char *fileName, int indexNo)
{
  string index_name = make_index_name (fileName, indexNo);
  PF::FileHandle index_file = this->pfm.OpenFile (index_name);
  IndexHandle index_handle (index_file);
  return index_handle;
}

void Manager::CloseIndex (IndexHandle& indexHandle)
{
  this->pfm.CloseFile (indexHandle.index_file);
}

IndexHandle::IndexHandle (PF::FileHandle& index_file)
  : index_file (index_file) {}

IndexHandle::IndexHandle (const IndexHandle& other)
  : index_file (other.index_file){}

PF::PageHandle IndexHandle::GetRoot () const
{
  PF::PageHandle page = this->index_file.GetFirstPage ();
  TreePage node (page.GetData ());
  while (! node.hdr->is_root) {
    PageNum next_num = page.GetPageNum ();
    this->index_file.UnpinPage (page);
    page = this->index_file.GetNextPage (next_num);
    new (&node) TreePage (page.GetData ());
  }
  return page;
}

PF::PageHandle IndexHandle::GetFirstLeaf () const
{
  PF::PageHandle page = this->GetRoot ();
  TreePage node (page.GetData ());
  while (! node.hdr->is_leaf) {
    PageNum next_num = node.page_nums [0];
    this->index_file.UnpinPage (page);
    page = this->index_file.GetPage (next_num);
    new (&node) TreePage (page.GetData ());
  }
  return page;
}

void IndexHandle::Insert (void *data, const RID& rid)
{
  PF::PageHandle root_page = this->GetRoot ();
  TreePage root (root_page.GetData ());

  ArrayElem key (root.hdr->key_type,
                 root.hdr->key_size,
                 (char*) data);
  KeyAndPageNum ret;
  try {
    new (&ret) KeyAndPageNum (root.insert (key, rid, this->index_file));
  }
  catch (exception& e) {
    this->index_file.UnpinPage (root_page);
    throw;
  }
  if (ret != NULL) {
    // Root split, need to create a new root.
    PF::PageHandle new_root_page = this->index_file.AllocatePage ();

    root.hdr->is_root = false;

    TreePageHdr hdr;
    hdr.num_keys = 1; // This is the key we are going to insert now
    hdr.is_root = true;
    hdr.is_leaf = false;
    hdr.key_size = root.hdr->key_size;
    hdr.key_type = root.hdr->key_type;

    TreePage new_root (new_root_page.GetData (), hdr);
    new_root.keys [0] = ret.key;
    new_root.page_nums [0] = root_page.GetPageNum ();
    new_root.page_nums [1] = ret.page_num;
    
    this->index_file.DoneWritingTo (new_root_page);
  }

  this->index_file.DoneWritingTo (root_page);
}

void IndexHandle::Delete (void *data, const RID& rid)
{
  PF::PageHandle root_page = this->GetRoot ();
  TreePage root (root_page.GetData ());
  ArrayElem key (root.hdr->key_type,
                 root.hdr->key_size,
                 (char*) data);
  try {
    root.Delete (key, rid, this->index_file);
  }
  catch (exception& e) {
    this->index_file.UnpinPage (root_page);
    throw;
  }
  this->index_file.UnpinPage (root_page);
}

void IndexHandle::ForcePages () const
{
  this->index_file.ForcePages ();
}

TreePage::TreePage (PF::PageHandle& pf_page)
  : TreePage::TreePage (pf_page.GetData ()) {}

TreePage::TreePage (char* start_of_page, const TreePageHdr& hdr)
{
  memcpy (start_of_page,
          (void *)(&hdr),
          sizeof (TreePageHdr));
  this->hdr = (TreePageHdr*) start_of_page;
  this->init (start_of_page);
  this->page_nums [this->hdr->num_keys] = -1;
}

TreePage::TreePage (char* start_of_page)
{
  this->hdr = (TreePageHdr*) start_of_page;
  this->init (start_of_page);
}

void TreePage::init (char* start_of_page)
{
  /*
    let there be 2n keys and 2n+1 page numbers. we know:

    2n * sizeof key + (2n + 1) * sizeof pgnum + header size <= pagesize

          pagesize - (header size + sizeof pgnum)
    2n <= ---------------------------------------
             sizeof key + sizeof pgnum
   */
  this->max_num_keys = PF::kPageSize - (sizeof (TreePageHdr) + sizeof (PageNum));
  this->max_num_keys /= this->hdr->key_size + sizeof (PageNum);

  // round to an even number.
  this->max_num_keys /= 2;
  this->max_num_keys *= 2;

  char* begin = start_of_page + sizeof (TreePageHdr);
  this->keys.init (begin,
                   this->hdr->key_size,
                   this->hdr->num_keys,
                   this->hdr->key_type,
                   this->max_num_keys);

  begin += this->max_num_keys * this->hdr->key_size;
  this->page_nums.init (begin,
                        sizeof (PageNum),
                        this->hdr->num_keys + 1,
                        INT,
                        this->max_num_keys + 1);
}

KeyAndPageNum::KeyAndPageNum (AttrType type,
                              int len,
                              char* data,
                              PageNum page_num):
  key (type, len, data), page_num (page_num) {}

KeyAndPageNum::KeyAndPageNum (const ArrayElem& key, PageNum page_num):
  key (key), page_num (page_num) {} 

KeyAndPageNum::KeyAndPageNum ():
  key (INT, 4, NULL), page_num (-1) {}

bool KeyAndPageNum::operator == (void* null)
{
  assert (null == NULL);
  return this->page_num == -1;
}

bool KeyAndPageNum::operator != (void* null)
{
  return !(*this == null);
}

bool TreePage::is_full () const
{
  return (this->keys.len () == this->max_num_keys);
}

void TreePage::Delete (const ArrayElem& key,
                       const RID& rid,
                       PF::FileHandle& index_file)
{
  int i;
  for (i = 0; i < this->keys.len(); ++i) {
    if (key <= this->keys [i]) break;
  }

  if (this->hdr->is_leaf) {
    if (i == this->keys.len()) throw error::RIDNoExist ();
    if (key != this->keys [i]) throw error::RIDNoExist ();
    PF::PageHandle bucket_page = index_file.GetPage (this->page_nums [i]);
    RIDPage bucket (bucket_page);
    try {
      bucket.Delete (rid, index_file);
    }
    catch (exception& e) {
      index_file.UnpinPage(bucket_page);
      throw;
    }
    index_file.DoneWritingTo (bucket_page);
  }
  else {
    PF::PageHandle child_page = index_file.GetPage (this->page_nums [i]);
    TreePage child (child_page);
    child.Delete (key, rid, index_file);
    index_file.UnpinPage (child_page);
  }
}

KeyAndPageNum TreePage::insert (const ArrayElem& key,
                                const RID& rid,
                                PF::FileHandle& index_file)
{
  KeyAndPageNum info_to_send_parent;
  if (this->hdr->is_leaf) {
    // search which bucket to insert into
    int i;
    for (i = 0; i < this->keys.len(); ++i) {
      if (key <= this->keys [i]) break;
    }

    if (i < this->keys.len() && this->keys [i] == key) {
      // We already have that key, just insert into the bucket.
      PF::PageHandle rid_page = index_file.GetPage (this->page_nums [i]);
      RIDPage bucket (rid_page);
      try {
        bucket.insert (rid, index_file);
      }
      catch (exception& e) {
        index_file.UnpinPage (rid_page);
        throw;
      }
      index_file.DoneWritingTo (rid_page);
    }
    else {
      // We are seeing this key for the first time, we need to create a bucket.
      PF::PageHandle new_bucket_page = index_file.AllocatePage ();
      RIDPage new_bucket (new_bucket_page);
      new_bucket.clear ();
      new_bucket.insert (rid, index_file);

      if (! this->is_full()) {
        // We already know that we do have space for one more key.
        // just add the key and page number in that case.
        this->add (i, key, i, new_bucket_page.GetPageNum ());
      }
      else {
        // This leaf is full, we need to split the leaf.
        PF::PageHandle new_leaf_page = index_file.AllocatePage ();
        this->transferkeys (new_leaf_page, this->max_num_keys / 2);
        TreePage new_leaf (new_leaf_page);

        // Last key of this node is what we always promote
        info_to_send_parent.key = this->keys [this->max_num_keys / 2 - 1];
        info_to_send_parent.page_num = new_leaf_page.GetPageNum ();
        
        // last page_num of this page is nothing but
        // the page_num of newly created page.
        this->page_nums [this->max_num_keys / 2] = new_leaf_page.GetPageNum ();

        if (i < this->max_num_keys / 2) {
          this->add (i, key, i, new_bucket_page.GetPageNum ());
        }
        else {
          int idx = i - this->max_num_keys / 2;
          new_leaf.add (idx, key, idx, new_bucket_page.GetPageNum ());
        }

        index_file.DoneWritingTo (new_leaf_page);
      }
      index_file.DoneWritingTo (new_bucket_page);
    }
  }
  else {
    // search which child to insert into
    int i;
    for (i = 0; i < this->keys.len(); ++i) {
      if (key <= this->keys [i]) break;
    }
    PF::PageHandle child_page = index_file.GetPage (this->page_nums [i]);
    TreePage child (child_page);
    KeyAndPageNum ret = child.insert (key, rid, index_file);
    if (ret == NULL) {
      // the child page did not split,
      // we don't have to do anything.
    }
    else {
      // The child page has split.

      // key returned by splitting of ith child
      // has to be smaller than ith key.
      if (i > 0) assert (ret.key >= this->keys[i-1]);
      if (i < this->keys.len()) assert (ret.key < this->keys[i]);

      if (! this->is_full ()) {
        this->add (i, ret.key, i+1, ret.page_num);
      }
      else {
        // create a sibling,
        PF::PageHandle sibling_page = index_file.AllocatePage ();

        //   does the new key go into the sibling?
        //   if yes, then, we send half - 1 keys to sibling
        //                 we keep half + 1 keys in self
        //   if no, then we send half keys to sibling
        //               we keep half keys to self
        //               we insert key into self, so now
        //               we have half + 1 keys in self
        if (i > (this->max_num_keys / 2)) {
          // The new key goes into the sibling
          this->transferkeys (sibling_page, this->max_num_keys / 2 - 1);
          TreePage sibling (sibling_page);
          sibling.keys.insert (this->max_num_keys / 2 - i - 1,
                               ret.key);
          sibling.page_nums.insert (this->max_num_keys / 2 - i,
                                    ret.page_num);
          sibling.hdr->num_keys++;
        }
        else {
          // The new key goes into the this page.
          this->transferkeys (sibling_page, this->max_num_keys / 2);
          this->add (i, ret.key, i+1, ret.page_num);
          
          TreePage sibling (sibling_page);
          sibling.page_nums [0] = this->page_nums [this->max_num_keys / 2];
        }

        info_to_send_parent.key = this->pop_key ();
        info_to_send_parent.page_num = sibling_page.GetPageNum ();

        index_file.DoneWritingTo (sibling_page);
      }
    }

    index_file.DoneWritingTo (child_page);
  }
  return info_to_send_parent;
}

void TreePage::add (int key_idx,
                    const ArrayElem& key,
                    int page_num_idx,
                    PageNum page_num)
{
  assert (! this->is_full());

  this->hdr->num_keys++;
  this->keys.insert (key_idx, key);
  this->page_nums.insert (page_num_idx, page_num);
}

ArrayElem TreePage::pop_key ()
{
  assert (this->hdr->num_keys > this->max_num_keys / 2);

  this->hdr->num_keys--;
  this->page_nums.pop ();
  return this->keys.pop ();
}

void TreePage::transferkeys (PF::PageHandle& sibling_page,
                             int num_keys_to_transfer)
{
  assert (this->is_full());

  this->hdr->is_root = false;   // a root never has a sibling

  TreePageHdr hdr;
  hdr.num_keys = 0;
  hdr.is_root = this->hdr->is_root;
  hdr.is_leaf = this->hdr->is_leaf;
  hdr.key_size = this->hdr->key_size;
  hdr.key_type = this->hdr->key_type;

  TreePage sibling (sibling_page.GetData(), hdr);
  int i = this->max_num_keys - num_keys_to_transfer;
  sibling.page_nums.insert (0, this->page_nums[i]);
  for (int j = 0; j < num_keys_to_transfer; ++j) {
    ArrayElem key = this->keys [i + j];
    PageNum pgnm = this->page_nums [i + j + 1];
    sibling.add (j, key, j+1, pgnm);
  }
  while (this->hdr->num_keys != this->max_num_keys - num_keys_to_transfer) {
    this->pop_key ();
  }
}

RIDPage::RIDPage (PF::PageHandle& page_handle)
{
  /* let the bitmap be n bytes
     there can be at max 8n RIDs
     n + 8n*RIDsize <= pgsize - hdrsize

           pgsize - hdrsize
     n <= ------------------
            8 RIDsize + 1
   */
  this->max_rid_count = 8 * ((PF::kPageSize - sizeof (RIDPageHdr))
                             / (8 * sizeof (RID) + 1));

  char* data = page_handle.GetData ();
  this->hdr = (RIDPageHdr *) data;
  data += sizeof (RIDPageHdr);

  new (&this->bitmap) Bitmap (this->max_rid_count, data);
  data += this->bitmap.num_bytes ();

  this->rids = (RID *) data;
}

void RIDPage::clear ()
{
  this->hdr->next_page = -1;
  this->hdr->rid_count = 0;
  this->bitmap.clear_all ();
}

bool RIDPage::is_full () const
{
  assert (this->hdr->rid_count <= this->max_rid_count);
  return this->hdr->rid_count == this->max_rid_count;
}

void RIDPage::insert (const RID& rid, PF::FileHandle& index_file)
{
  // TODO(sujeet): This is very inefficient. Now the free map
  //               is almost useless. Use a hash table maybe?
  for (int i = 0; i < this->max_rid_count; ++i) {
    if (this->bitmap.get (i) && this->rids [i] == rid)
      throw error::DuplicateRID();
  }

  if (this->is_full()) {
    if (this->hdr->next_page == -1) {
      PF::PageHandle new_rid_page = index_file.AllocatePage ();
      RIDPage new_bucket (new_rid_page);
      new_bucket.clear ();
      new_bucket.add (rid);
      this->hdr->next_page = new_rid_page.GetPageNum ();
      index_file.DoneWritingTo (new_rid_page);
    }
    else {
      PF::PageHandle next_page = index_file.GetPage (this->hdr->next_page);
      RIDPage next_bucket (next_page);
      next_bucket.insert (rid, index_file);
      index_file.DoneWritingTo (next_page);
    }
  }
  else {
    this->add (rid);
  }
}

void RIDPage::add (const RID& rid)
{
  assert (! this->is_full());

  int i = this->bitmap.getFirstUnset ();
  this->rids [i] = rid;
  this->bitmap.set (i);
  this->hdr->rid_count++;
}

void RIDPage::Delete (const RID& rid, PF::FileHandle& index_file)
{
  for (int i = 0; i < this->max_rid_count; ++i) {
    if (this->bitmap.get (i) && this->rids [i] == rid) {
      this->bitmap.clear (i);
      return;
    }
  }
  if (this->hdr->next_page != -1) {
    PF::PageHandle next_page = index_file.GetPage (this->hdr->next_page);
    RIDPage next_bucket (next_page);
    next_bucket.Delete (rid, index_file);
    index_file.DoneWritingTo (next_page);
  }
  else {
    // We searched everywhere, didn't find the entry! :(
    throw error::RIDNoExist();
  }
}

const RID Scan::end (-1, -1);

Scan::Scan (const IndexHandle& indexHandle,
            CompOp compOp,
            void* value)
{
  PF::PageHandle leaf_page = indexHandle.GetFirstLeaf ();
  TreePage leaf (leaf_page);

  this->index_file = indexHandle.index_file;

  this->compOp = compOp;
  new (&this->key) ArrayElem (leaf.hdr->key_type,
                              leaf.hdr->key_size,
                              (char*) (value ? value : this->dummy));
  // this->key = key;
  this->index_file.UnpinPage (leaf_page);
  this->leaf_page_num = leaf_page.GetPageNum ();
  this->key_i = -1;
  this->next_key_i ();
  // Now both leaf_page_num and key_i are set.

  leaf_page = this->index_file.GetPage (this->leaf_page_num);
  new (&leaf) TreePage (leaf_page);
  this->bucket_page_num = leaf.page_nums [this->key_i];
  this->rid_i = -1;
  this->next_rid_i ();
  this->index_file.UnpinPage (leaf_page);
  // Now all the four parametrs for scan are set.
}

void Scan::next_leaf_page_num ()
{
  PF::PageHandle leaf_page = this->index_file.GetPage (this->leaf_page_num);
  TreePage leaf (leaf_page);
  this->leaf_page_num = leaf.page_nums [leaf.hdr->num_keys];
  this->index_file.UnpinPage (leaf_page);
  return;
}

bool Scan::satisfy (const ArrayElem& key)
{
  switch (this->compOp) {
    case NO_OP: return true;
    case EQ_OP: return this->key == key;
    case LT_OP: return this->key < key;
    case GT_OP: return this->key > key;
    case LE_OP: return this->key <= key;
    case GE_OP: return this->key >= key;
    case NE_OP: return this->key != key;
  }
  return true;
}

void Scan::next_key_i ()
{
  this->key_i++;

  PF::PageHandle leaf_page = this->index_file.GetPage (this->leaf_page_num);
  TreePage leaf (leaf_page);
  int max_count = leaf.hdr->num_keys;

  while (this->key_i < max_count &&
         not this->satisfy (leaf.keys [this->key_i])) this->key_i++;
  this->index_file.UnpinPage (leaf_page);

  if (this->key_i < max_count) return;

  this->key_i = -1;
  this->next_leaf_page_num ();
  if (this->leaf_page_num == -1) return;
  else this->next_key_i ();
}

void Scan::next_bucket_page_num ()
{
  PF::PageHandle bucket_page = this->index_file.GetPage (this->bucket_page_num);
  RIDPage bucket (bucket_page);
  this->bucket_page_num = bucket.hdr->next_page;
  this->index_file.UnpinPage (bucket_page);

  if (this->bucket_page_num != -1) return;

  this->next_key_i ();
  if (this->leaf_page_num == -1) return;

  PF::PageHandle leaf_page = this->index_file.GetPage (this->leaf_page_num);
  TreePage leaf (leaf_page);
  this->bucket_page_num = leaf.page_nums [this->key_i];
  this->index_file.UnpinPage (leaf_page);
}

void Scan::next_rid_i ()
{
  this->rid_i++;

  PF::PageHandle bucket_page = this->index_file.GetPage (this->bucket_page_num);
  RIDPage bucket (bucket_page);
  int max_count = bucket.max_rid_count;
  while (this->rid_i < max_count &&
         not bucket.bitmap.get (this->rid_i)) this->rid_i++;
  this->index_file.UnpinPage (bucket_page);

  if (this->rid_i < max_count) return;
  
  this->rid_i = -1;
  this->next_bucket_page_num ();
  if (this->leaf_page_num == -1) return;
  else this->next_rid_i ();
}

RID Scan::next ()
{
  if (this->leaf_page_num == -1) {
    RID rid (-1, -1);
    return rid;
  }
  else {
    PF::PageHandle bucket_page = this->index_file.GetPage (this->bucket_page_num);
    RIDPage bucket (bucket_page);
    RID rid = bucket.rids [this->rid_i];
    this->index_file.UnpinPage (bucket_page);

    this->next_rid_i ();

    return rid;
  }
}

} // namespace IX
