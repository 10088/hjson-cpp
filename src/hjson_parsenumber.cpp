#include "hjson.h"
#include <cmath>
#if (__cplusplus >= 201703L || _MSVC_LANG >= 201703L) && __has_include(<charconv>)
# define HAS_CHARCONV 1
# include <charconv>
#else
# define HAS_CHARCONV 0
# include <sstream>
#endif


namespace Hjson {


struct Parser {
  const unsigned char *data;
  size_t dataSize;
  int at;
  unsigned char ch;
};


static bool _parseFloat(double *pNumber, const char *pCh, size_t nCh) {
#if HAS_CHARCONV
  auto res = std::from_chars(pCh, pCh + nCh, *pNumber);

  return res.ptr == pCh + nCh && res.ec != std::errc::result_out_of_range &&
    !std::isinf(*pNumber) && !std::isnan(*pNumber);
#else
  std::string str(pCh, nCh);
  std::stringstream ss(str);

  // Make sure we expect dot (not comma) as decimal point.
  ss.imbue(std::locale::classic());

  ss >> *pNumber;

  return ss.eof() && !ss.fail() && !std::isinf(*pNumber) && !std::isnan(*pNumber);
#endif
}


static bool _parseInt(std::int64_t *pNumber, const char *pCh, size_t nCh) {
#if HAS_CHARCONV
  auto res = std::from_chars(pCh, pCh + nCh, *pNumber);

  return res.ptr == pCh + nCh && res.ec != std::errc::result_out_of_range;
#else
  std::string str(pCh, nCh);
  std::stringstream ss(str);

  ss >> *pNumber;

  return ss.eof() && !ss.fail();
#endif
}


static bool _next(Parser *p) {
  // get the next character.
  if (p->at < p->dataSize) {
    p->ch = p->data[p->at++];
    return true;
  }

  if (p->at == p->dataSize) {
    p->at++;
    p->ch = 0;
  }

  return false;
}


// Parse a number value.
bool tryParseNumber(Value *pValue, const char *text, size_t textSize, bool stopAtNext) {
  Parser p = {
    (const unsigned char*) text,
    textSize,
    0,
    ' '
  };

  int leadingZeros = 0;
  bool testLeading = true;

  _next(&p);

  if (p.ch == '-') {
    _next(&p);
  }

  while (p.ch >= '0' && p.ch <= '9') {
    if (testLeading) {
      if (p.ch == '0') {
        leadingZeros++;
      } else {
        testLeading = false;
      }
    }
    _next(&p);
  }

  if (testLeading) {
    leadingZeros--;
  } // single 0 is allowed

  if (p.ch == '.') {
    while (_next(&p) && p.ch >= '0' && p.ch <= '9') {
    }
  }
  if (p.ch == 'e' || p.ch == 'E') {
    _next(&p);
    if (p.ch == '-' || p.ch == '+') {
      _next(&p);
    }
    while (p.ch >= '0' && p.ch <= '9') {
      _next(&p);
    }
  }

  auto end = p.at;

  // skip white/to (newline)
  while (p.ch > 0 && p.ch <= ' ') {
    _next(&p);
  }

  if (stopAtNext) {
    // end scan if we find a punctuator character like ,}] or a comment
    if (p.ch == ',' || p.ch == '}' || p.ch == ']' ||
      p.ch == '#' || (p.ch == '/' && (p.data[p.at] == '/' || p.data[p.at] == '*')))
    {
      p.ch = 0;
    }
  }

  if (p.ch > 0 || leadingZeros != 0) {
    // Invalid number.
    return false;
  }

  std::int64_t i;
  if (_parseInt(&i, (char*) p.data, end - 1)) {
    *pValue = Value(i, Int64_tag{});
    return true;
  } else {
    double d;
    if (_parseFloat(&d, (char*) p.data, end - 1)) {
      *pValue = Value(d);
      return true;
    }
  }

  return false;
}


bool startsWithNumber(const char *text, size_t textSize) {
  Value number;
  return tryParseNumber(&number, text, textSize, true);
}


}
