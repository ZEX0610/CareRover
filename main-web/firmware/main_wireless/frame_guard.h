#pragma once
#include <cstddef>
#include <cmath>
namespace carerover {
// Bound recursion before entering cJSON's recursive parser. Syntax validation
// itself remains cJSON's responsibility. Braces inside strings are not nesting.
inline bool boundedJsonText(const char* data, size_t length) {
  if (!data || !length || length > 65536) return false;
  int depth = 0;
  bool quoted = false, escaped = false;
  for (size_t i = 0; i < length; ++i) {
    const unsigned char c = data[i];
    if (!c) return false;
    if (quoted) {
      if (escaped) escaped = false;
      else if (c == '\\') escaped = true;
      else if (c == '"') quoted = false;
    } else if (c == '"') quoted = true;
    else if (c == '{' || c == '[') { if (++depth > 8) return false; }
    else if (c == '}' || c == ']') { if (--depth < 0) return false; }
  }
  return !quoted && depth == 0;
}
inline bool commandAgeAllowed(double ageMs) {
  return std::isfinite(ageMs) && ageMs <= 200 && ageMs >= -100;
}
} // namespace carerover
