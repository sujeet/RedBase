#include "Array.h"
#include "redbase.h"

#include "gtest/gtest.h"

#include <iostream>

using namespace std;

TEST (Array, init)
{
  int a [] = {1, 2, 3, -1};
  
  Array array ((char*)a, 4, 3, INT, 4); // 1, 2, 3


  EXPECT_TRUE (array[0] == 1);
  EXPECT_TRUE (array[1] == 2);
  EXPECT_TRUE (array[2] == 3);

  array[2] = 9;                 //  1, 2, 9
  EXPECT_TRUE (array[0] == 1);
  EXPECT_TRUE (array[1] == 2);
  EXPECT_TRUE (array[2] == 9);
  EXPECT_TRUE (array[2] != 3);

  array.insert (1, 10);          // 1, 10, 2, 9
  EXPECT_TRUE (array[0] == 1);
  EXPECT_TRUE (array[1] == 10);
  EXPECT_TRUE (array[2] == 2);
  EXPECT_TRUE (array[3] == 9);
}

TEST (Array, comparisons)
{
  int a[] = {0};
  Array array ((char*)a, 4, 1, INT, 1);
  
  array[0] = -1;
  EXPECT_TRUE (array[0] < 0);
  EXPECT_TRUE (array[0] <= 0);
  EXPECT_TRUE (array[0] != 0);

  EXPECT_FALSE (array[0] >= 0);
  EXPECT_FALSE (array[0] > 0);
  EXPECT_FALSE (array[0] == 0);

  array[0] = 1;
  EXPECT_TRUE (array[0] > 0);
  EXPECT_TRUE (array[0] >= 0);
  EXPECT_TRUE (array[0] != 0);

  EXPECT_FALSE (array[0] <= 0);
  EXPECT_FALSE (array[0] < 0);
  EXPECT_FALSE (array[0] == 0);

  array[0] = 0;
  EXPECT_TRUE (array[0] >= 0);
  EXPECT_TRUE (array[0] <= 0);
  EXPECT_TRUE (array[0] == 0);

  EXPECT_FALSE (array[0] < 0);
  EXPECT_FALSE (array[0] > 0);
  EXPECT_FALSE (array[0] != 0);
}

TEST (Array, insert_last)
{
  int a [] = {-1, -1};
  
  Array array ((char*)a, 4, 0, INT, 2);
  EXPECT_TRUE (array.len() == 0);

  array.insert (0, 1);
  EXPECT_TRUE (array.len() == 1);
  EXPECT_TRUE (array[0] == 1);

  array.insert (1, 99);
  EXPECT_TRUE (array.len() == 2);
  EXPECT_TRUE (array[0] == 1);
  EXPECT_TRUE (array[1] == 99);
}

TEST (Array, pop)
{
  int a [] = {99, 999};
  
  Array array ((char*)a, 4, 2, INT, 2);

  EXPECT_TRUE (array.len() == 2);

  EXPECT_TRUE (array.pop() == 999);
  EXPECT_TRUE (array.len() == 1);

  EXPECT_TRUE (array.pop() == 99);
  EXPECT_TRUE (array.len() == 0);
}
