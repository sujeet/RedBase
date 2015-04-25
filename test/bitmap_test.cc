#include "bitmap.h"
#include "gtest/gtest.h"

TEST (Bitmap, init)
{
  unsigned char chars[2];
  Bitmap bitmap (11, chars);
  bitmap.set (0);
  bitmap.clear (1);
  bitmap.clear (2);
  bitmap.set (3);
  bitmap.set (4);
  bitmap.set (5);
  bitmap.set (6);
  bitmap.clear (7);
  bitmap.set (8);
  bitmap.clear (9);
  bitmap.set (10);

  EXPECT_TRUE (bitmap.get (0));
  EXPECT_FALSE (bitmap.get (1));
  EXPECT_FALSE (bitmap.get (2));
  EXPECT_TRUE (bitmap.get (3));
  EXPECT_TRUE (bitmap.get (4));
  EXPECT_TRUE (bitmap.get (5));
  EXPECT_TRUE (bitmap.get (6));
  EXPECT_FALSE (bitmap.get (7));
  EXPECT_TRUE (bitmap.get (8));
  EXPECT_FALSE (bitmap.get (9));
  EXPECT_TRUE (bitmap.get (10));
}

TEST (Bitmap, getFirstUnset)
{
  unsigned char chars[2] = {0xff, 0x00};
  Bitmap bitmap (11, chars);
  EXPECT_EQ (8, bitmap.getFirstUnset());

  bitmap.set (8);
  bitmap.set (9);

  EXPECT_EQ (10, bitmap.getFirstUnset());
  
  bitmap.clear (5);
  
  EXPECT_EQ (5, bitmap.getFirstUnset());
}
