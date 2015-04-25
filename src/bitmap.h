#ifndef BITMAP_H
#define BITMAP_H

class Bitmap
{
private:
  int _num_bits;
  unsigned char *map;
public:
  Bitmap(const int num_bits, unsigned char *map);
  bool get(const int bit_index) const;
  int getFirstUnset() const;
  void set(const int bit_index);
  void clear(const int bit_index);
  int num_bits() const;
  int num_bytes() const;
};

#endif  // BITMAP_H
