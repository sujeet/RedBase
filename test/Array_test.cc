#include "Array.h"
#include "redbase.h"

#include "gtest/gtest.h"

TEST (Array, init)
{
  const char* stack_strings = "ab" "bc" "ab" "aa" "__";
  char* strings = new char[10];
  for (int i = 0; i < 10; ++i) strings[i] = stack_strings[i];
  
  Array array (strings, 2, 4, STRING);

  EXPECT_TRUE (array[0] < array[1]);
  EXPECT_TRUE (array[0] > array[3]);
  EXPECT_TRUE (array[0] == array[2]);

  array.set (0, "aa");
  EXPECT_TRUE (array[0] == array[3]);

  array.insert (2, "bc");
  EXPECT_TRUE (array[1] == array[2]);
  EXPECT_TRUE (array[0] != array[3]);
  EXPECT_TRUE (array[0] == array[4]);

  delete strings;
}
