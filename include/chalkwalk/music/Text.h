#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

// The handful of string operations the notation layer needs.
//
// Deliberately small. This is not a string library: it is the operations two
// headers actually perform, and no more. `Tuning.h`'s `detail` has its own trim
// and split for Scala files; the two sets are kept apart until something needs
// both, because merging them now would be designing for a caller that does not
// exist.

namespace chalkwalk::music::text {

inline bool isAsciiDigit(char c) { return c >= '0' && c <= '9'; }

inline char lowerChar(char c) {
  return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}
inline char upperChar(char c) {
  return (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
}

inline std::string lower(std::string_view s) {
  std::string out(s);
  for (auto &c : out)
    c = lowerChar(c);
  return out;
}

inline std::string upper(std::string_view s) {
  std::string out(s);
  for (auto &c : out)
    c = upperChar(c);
  return out;
}

// Whitespace from both ends, which is what every caller here means by "trim".
inline std::string trim(std::string_view s) {
  const auto ws = [](char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
  };
  size_t b = 0, e = s.size();
  while (b < e && ws(s[b]))
    ++b;
  while (e > b && ws(s[e - 1]))
    --e;
  return std::string(s.substr(b, e - b));
}

inline bool startsWith(std::string_view s, std::string_view prefix) {
  return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

inline bool startsWithIgnoreCase(std::string_view s, std::string_view prefix) {
  return startsWith(lower(s), lower(prefix));
}

inline bool contains(std::string_view s, std::string_view what) {
  return s.find(what) != std::string_view::npos;
}

// Index of a character, or -1. Signed on purpose: every caller here compares
// against a negative to mean "not found", and `npos` compares as enormous.
inline int indexOf(std::string_view s, char c) {
  const auto at = s.find(c);
  return at == std::string_view::npos ? -1 : (int)at;
}

inline int lastIndexOf(std::string_view s, char c) {
  const auto at = s.rfind(c);
  return at == std::string_view::npos ? -1 : (int)at;
}

inline int indexOfIgnoreCase(std::string_view s, std::string_view what) {
  const auto at = lower(s).find(lower(what));
  return at == std::string_view::npos ? -1 : (int)at;
}

// Every run of the given delimiters, with empty pieces dropped -- which is what
// a tokeniser with an empty quote set does, and what the
// chart and scale parsers both rely on.
inline std::vector<std::string> split(std::string_view s,
                                      std::string_view delimiters) {
  std::vector<std::string> out;
  std::string current;
  for (char c : s) {
    if (delimiters.find(c) != std::string_view::npos) {
      if (!current.empty())
        out.push_back(current);
      current.clear();
    } else {
      current += c;
    }
  }
  if (!current.empty())
    out.push_back(current);
  return out;
}

inline std::string join(const std::vector<std::string> &parts,
                        std::string_view separator) {
  std::string out;
  for (size_t i = 0; i < parts.size(); ++i) {
    if (i != 0)
      out += separator;
    out += parts[i];
  }
  return out;
}

inline std::string withoutChars(std::string_view s, std::string_view drop) {
  std::string out;
  out.reserve(s.size());
  for (char c : s)
    if (drop.find(c) == std::string_view::npos)
      out += c;
  return out;
}

} // namespace chalkwalk::music::text
