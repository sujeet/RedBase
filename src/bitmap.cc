#include "bitmap.h"

#include <cstdio>
#include <cstring>
#include <cassert>
#include <iostream>

using namespace std;

int Bitmap::num_bytes() const
{
  return (this->_num_bits + 7) / 8;
}

int Bitmap::num_bits() const
{
  return this->num_bytes() * 8;
}

Bitmap::Bitmap()
  : _num_bits(0), map(NULL) {}

Bitmap::Bitmap(const int _num_bits, unsigned char *map)
  : _num_bits(_num_bits), map(map){}

Bitmap::Bitmap(const int _num_bits, char *map)
  : _num_bits(_num_bits), map((unsigned char*)map){}

bool Bitmap::get(const int bit_index) const
{
  assert (bit_index < this->num_bits());

  int byte_i = bit_index / 8;
  int bit_i = bit_index % 8;
  return this->map[byte_i] & (1 << bit_i);
}

void Bitmap::set(const int bit_index)
{
  if (bit_index >= this->num_bits())
  cout << "index : " << bit_index << "\nnum bits : " << this->num_bits() << endl;
  assert (bit_index < this->num_bits());

  int byte_i = bit_index / 8;
  int bit_i = bit_index % 8;
  this->map[byte_i] |= (1 << bit_i);
}

void Bitmap::clear_all ()
{
  memset (this->map, 0, this->num_bytes ());
}

void Bitmap::clear(const int bit_index)
{
  assert (bit_index < this->num_bits());

  int byte_i = bit_index / 8;
  int bit_i = bit_index % 8;
  this->map[byte_i] &= ~(1 << bit_i);
}

int Bitmap::getFirstUnset() const
{
  for (int byte_i = 0; byte_i < this->num_bytes(); ++byte_i) {
    unsigned char byte = this->map[byte_i];
    if (byte != 0xff) {
      for (int bit_i = 0; bit_i < 8; ++bit_i) {
        if (byte % 2 == 0) {
          return byte_i * 8 + bit_i;
        }
        else {
          byte /= 2;
        }
      }
    }
  }
  return -1;
}
