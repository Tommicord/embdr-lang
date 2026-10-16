/* Copyright(c) 2026 Tommicord
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the “Software”), to deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#include <catch2/catch_all.hpp>

import embdr.lex.charinfo;

using namespace embdr::lex;

constexpr bool
ascii_classification_holds () noexcept
{
  for (int i = 0; i < 128; ++i)
    {
      const char c = static_cast<char> (i);
      const bool alpha = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
      const bool digit = c >= '0' && c <= '9';
      if (charinfo::is_alpha (c) != alpha)
        return false;
      if (charinfo::is_digit (c) != digit)
        return false;
      if (charinfo::is_alnum (c) != (alpha || digit))
        return false;
      if (!charinfo::is_ascii (c))
        return false;
    }
  return true;
}

static_assert (ascii_classification_holds (), "charinfo predicates must classify ASCII exactly");
static_assert (charinfo::is_identifier_start ('_') && !charinfo::is_identifier_start ('1'));
static_assert (charinfo::is_identifier_body ('1') && !charinfo::is_identifier_body ('$'));
static_assert (charinfo::is_hex_digit ('f') && charinfo::is_hex_digit ('F') && !charinfo::is_hex_digit ('g'));
static_assert (charinfo::is_octal_digit ('7') && !charinfo::is_octal_digit ('8'));
static_assert (charinfo::base_digit_value ('9') == 9 && charinfo::base_digit_value ('a') == 10
               && charinfo::base_digit_value ('F') == 15);

TEST_CASE ("ASCII classification", "[charinfo]")
{
  REQUIRE (charinfo::is_ascii ('a'));
  REQUIRE (charinfo::is_ascii ('\x7F'));
  REQUIRE (!charinfo::is_ascii ('\x80'));
  REQUIRE (!charinfo::is_ascii ('\xFF'));

  REQUIRE (charinfo::is_alpha ('z'));
  REQUIRE (charinfo::is_alpha ('Z'));
  REQUIRE (!charinfo::is_alpha ('0'));
  REQUIRE (!charinfo::is_alpha ('_'));

  REQUIRE (charinfo::is_alnum ('5'));
  REQUIRE (!charinfo::is_alnum ('#'));

  REQUIRE (charinfo::is_digit ('0'));
  REQUIRE (!charinfo::is_digit ('/'));
  REQUIRE (!charinfo::is_digit (':'));
}

TEST_CASE ("hex and octal digit classification", "[charinfo]")
{
  for (const char c : {'0', '1', '7', '9', 'a', 'f', 'A', 'F'})
    {
      REQUIRE (charinfo::is_hex_digit (c));
    }
  REQUIRE (!charinfo::is_hex_digit ('g'));
  REQUIRE (!charinfo::is_hex_digit ('G'));
  REQUIRE (!charinfo::is_hex_digit (' '));

  REQUIRE (charinfo::is_octal_digit ('0'));
  REQUIRE (charinfo::is_octal_digit ('7'));
  REQUIRE (!charinfo::is_octal_digit ('8'));
  REQUIRE (!charinfo::is_octal_digit ('9'));
}

TEST_CASE ("base digit values", "[charinfo]")
{
  REQUIRE (charinfo::base_digit_value ('0') == 0);
  REQUIRE (charinfo::base_digit_value ('9') == 9);
  REQUIRE (charinfo::base_digit_value ('a') == 10);
  REQUIRE (charinfo::base_digit_value ('f') == 15);
  REQUIRE (charinfo::base_digit_value ('A') == 10);
  REQUIRE (charinfo::base_digit_value ('F') == 15);
}

TEST_CASE ("identifier characters", "[charinfo]")
{
  REQUIRE (charinfo::is_identifier_start ('a'));
  REQUIRE (charinfo::is_identifier_start ('Z'));
  REQUIRE (charinfo::is_identifier_start ('_'));
  REQUIRE (!charinfo::is_identifier_start ('9'));
  REQUIRE (!charinfo::is_identifier_start ('\x80'));

  REQUIRE (charinfo::is_identifier_body ('a'));
  REQUIRE (charinfo::is_identifier_body ('9'));
  REQUIRE (charinfo::is_identifier_body ('_'));
  REQUIRE (!charinfo::is_identifier_body ('+'));
}

TEST_CASE ("whitespace classification separates line breaks from horizontal space", "[charinfo]")
{
  REQUIRE (charinfo::is_horizontal_space (' '));
  REQUIRE (charinfo::is_horizontal_space ('\t'));
  REQUIRE (charinfo::is_horizontal_space ('\v'));
  REQUIRE (charinfo::is_horizontal_space ('\f'));
  REQUIRE (!charinfo::is_horizontal_space ('\n'));
  REQUIRE (!charinfo::is_horizontal_space ('\r'));

  REQUIRE (charinfo::is_newline ('\n'));
  REQUIRE (charinfo::is_newline ('\r'));
  REQUIRE (!charinfo::is_newline (' '));

  REQUIRE (charinfo::is_space ('\n'));
  REQUIRE (charinfo::is_space ('\r'));
  REQUIRE (charinfo::is_space (' '));
  REQUIRE (!charinfo::is_space ('x'));
}

TEST_CASE ("punctuation is printable ASCII excluding space", "[charinfo]")
{
  REQUIRE (charinfo::is_punctuation ('+'));
  REQUIRE (charinfo::is_punctuation ('~'));
  REQUIRE (!charinfo::is_punctuation (' '));
  REQUIRE (!charinfo::is_punctuation ('\t'));
  REQUIRE (!charinfo::is_punctuation ('\n'));
  REQUIRE (!charinfo::is_punctuation ('a'));
  REQUIRE (!charinfo::is_punctuation ('\0'));
}

TEST_CASE ("is_base_digit covers every supported radix and rejects the rest", "[charinfo]")
{
  REQUIRE (charinfo::is_base_digit ('1', 2));
  REQUIRE (!charinfo::is_base_digit ('2', 2));
  REQUIRE (charinfo::is_base_digit ('7', 8));
  REQUIRE (!charinfo::is_base_digit ('8', 8));
  REQUIRE (charinfo::is_base_digit ('9', 10));
  REQUIRE (charinfo::is_base_digit ('f', 16));
  REQUIRE (!charinfo::is_base_digit ('g', 16));
  // an unsupported base classifies nothing
  REQUIRE (!charinfo::is_base_digit ('0', 3));
  REQUIRE (!charinfo::is_base_digit ('0', 0));
}
