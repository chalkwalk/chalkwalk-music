// SPDX-License-Identifier: MIT
// Part of chalkwalk-music. See LICENSE.
#pragma once

// Scala tuning: .scl scale files and .kbm keyboard mappings.
//
// The Scala format is the lingua franca of microtonality -- Manuel Op de
// Coul's archive runs to thousands of scales -- and it is thinly served in
// C++. What exists is usually welded to a synth or a framework.
//
// **This parser takes TEXT, not a filename.** That is deliberate and it is the
// main design decision here. A parser that opens files cannot be tested without
// a filesystem, cannot read a scale out of a zip, a resource bundle, an HTTP
// response or a string literal in a test, and drags a file API into a library
// that otherwise needs nothing. Reading the bytes is the caller's job, and it
// is one line.
//
// Two quirks of the format that this handles and that a naive reader gets
// wrong:
//
//   - **The last entry of a .scl file is the PERIOD, not a note.** For an
//     octave-repeating scale it is 2/1 (1200 cents). It is not necessarily an
//     octave: Bohlen-Pierce repeats at 3/1, and stretched piano tunings repeat
//     slightly wide. `computeTable` uses the last entry as the repeat interval
//     rather than assuming 1200.
//   - **The implied unison is not written down.** A 12-note scale file lists
//     12 entries, which are degrees 1..12; degree 0 at 0 cents is implicit.
//
// Numeric parsing is deliberately LENIENT, matching the behaviour these files
// are read with in practice: a line that does not start with a number reads as
// zero rather than throwing. Scala files in the wild carry stray text.
//
// JUCE-free, dependency-free, header-only.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cctype>
#include <cstdlib>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace chalkwalk::music {

// A tuning as a per-MIDI-note deviation from 12-TET, in cents.
//
// Deviation rather than absolute frequency, because that is what a synth
// actually needs: it already knows how to play note 60, and this says how far
// to bend it. It also makes the identity tuning exactly "all zeros", which is
// cheap to detect and cheap to skip.
struct TuningTable {
  std::array<float, 128> centsDeviation{};
  int stepsPerOctave = 12;
  std::string name;

  [[nodiscard]] bool isIdentity() const {
    return std::all_of(centsDeviation.begin(), centsDeviation.end(),
                       [](float c) { return std::fabs(c) <= 0.001f; });
  }
};

// A .kbm keyboard mapping: which MIDI keys sound which scale degrees, and
// where the tuning is pinned to a real frequency.
struct KeyboardMapping {
  int mapSize = 12;
  int firstNote = 0;
  int lastNote = 127;
  int middleNote = 60;   // the key that sounds scale degree 0
  int refNote = 69;      // the key whose frequency is pinned
  double refFreq = 440.0;
  int repeatDegree = 0;  // degree at which the period repeats; 0 = scale size
  std::vector<int> mapping;  // -1 marks an unmapped key ('x' in the file)
};

namespace detail {

inline std::string trim(std::string_view s) {
  const auto notSpace = [](unsigned char c) {
    return c != ' ' && c != '\t' && c != '\r' && c != '\n' && c != '\f' &&
           c != '\v';
  };
  std::size_t b = 0;
  while (b < s.size() && !notSpace(static_cast<unsigned char>(s[b])))
    ++b;
  std::size_t e = s.size();
  while (e > b && !notSpace(static_cast<unsigned char>(s[e - 1])))
    --e;
  return std::string(s.substr(b, e - b));
}

inline std::vector<std::string> splitLines(std::string_view text) {
  std::vector<std::string> out;
  std::size_t start = 0;
  for (std::size_t i = 0; i <= text.size(); ++i) {
    if (i == text.size() || text[i] == '\n') {
      out.emplace_back(text.substr(start, i - start));
      start = i + 1;
    }
  }
  return out;
}

// Lenient, and deliberately so: parse a leading number and yield zero when
// there is none. Scala files in the wild carry stray text on data lines, and a
// reader that throws on them is a reader that cannot open the archive.
inline int toInt(const std::string &s) {
  return static_cast<int>(std::strtol(s.c_str(), nullptr, 10));
}

inline double toDouble(const std::string &s) {
  return std::strtod(s.c_str(), nullptr);
}

inline double ratioToCents(double numerator, double denominator) {
  return 1200.0 * std::log2(numerator / denominator);
}

// A .scl pitch is either a ratio ("3/2") or a cents value ("701.955"). The
// convention is that a value containing a '.' is cents and one containing '/'
// is a ratio; a bare integer is a ratio over 1.
inline bool parseRatio(const std::string &token, double &outCents) {
  const auto slash = token.find('/');
  if (slash == std::string::npos)
    return false;
  const double num = toDouble(token.substr(0, slash));
  const double den = toDouble(token.substr(slash + 1));
  if (den == 0.0)
    return false;
  outCents = ratioToCents(num, den);
  return true;
}

inline bool equalsIgnoreCase(const std::string &a, const char *b) {
  std::size_t i = 0;
  for (; i < a.size() && b[i] != '\0'; ++i)
    if (std::tolower(static_cast<unsigned char>(a[i])) !=
        std::tolower(static_cast<unsigned char>(b[i])))
      return false;
  return i == a.size() && b[i] == '\0';
}

}  // namespace detail

// Parse the pitch list out of .scl text. `nameOut` receives the description
// line. The returned vector is degrees 1..N in cents; degree 0 is implicit,
// and the LAST entry is the period.
[[nodiscard]] inline std::vector<double> parseScl(std::string_view sclText,
                                                  std::string &nameOut) {
  std::vector<double> result;
  nameOut.clear();

  int phase = 0;  // 0 = want description, 1 = want count, 2 = reading pitches
  int expectedN = 0;

  for (const auto &raw : detail::splitLines(sclText)) {
    std::string line = detail::trim(raw);
    if (!line.empty() && line[0] == '!')
      continue;

    if (phase == 0) {
      nameOut = line;
      phase = 1;
      continue;
    }
    if (phase == 1) {
      expectedN = detail::toInt(line);
      phase = 2;
      continue;
    }
    if (line.empty())
      continue;

    const auto comment = line.find('!');
    if (comment != std::string::npos)
      line = detail::trim(line.substr(0, comment));
    if (line.empty())
      continue;

    double cents = 0.0;
    if (!detail::parseRatio(line, cents))
      cents = detail::toDouble(line);
    result.push_back(cents);
    if (static_cast<int>(result.size()) >= expectedN)
      break;
  }
  return result;
}

// Parse .kbm text. Seven header values, then one mapping entry per line.
[[nodiscard]] inline KeyboardMapping parseKbm(std::string_view kbmText) {
  KeyboardMapping kbm;

  std::vector<std::string> data;
  for (const auto &raw : detail::splitLines(kbmText)) {
    std::string line = detail::trim(raw);
    if (line.empty() || line[0] == '!')
      continue;
    const auto comment = line.find('!');
    if (comment != std::string::npos)
      line = detail::trim(line.substr(0, comment));
    if (!line.empty())
      data.push_back(line);
  }

  if (data.size() >= 1) kbm.mapSize      = detail::toInt(data[0]);
  if (data.size() >= 2) kbm.firstNote    = detail::toInt(data[1]);
  if (data.size() >= 3) kbm.lastNote     = detail::toInt(data[2]);
  if (data.size() >= 4) kbm.middleNote   = detail::toInt(data[3]);
  if (data.size() >= 5) kbm.refNote      = detail::toInt(data[4]);
  if (data.size() >= 6) kbm.refFreq      = detail::toDouble(data[5]);
  if (data.size() >= 7) kbm.repeatDegree = detail::toInt(data[6]);

  for (std::size_t i = 7; i < data.size(); ++i)
    kbm.mapping.push_back(detail::equalsIgnoreCase(data[i], "x")
                              ? -1
                              : detail::toInt(data[i]));
  return kbm;
}

// The mapping implied by a scale with no .kbm: every degree in order, middle C
// at degree 0, A440 as the reference.
[[nodiscard]] inline KeyboardMapping defaultKbm(int scaleSize) {
  KeyboardMapping kbm;
  kbm.mapSize = scaleSize;
  kbm.firstNote = 0;
  kbm.lastNote = 127;
  kbm.middleNote = 60;
  kbm.refNote = 69;
  kbm.refFreq = 440.0;
  kbm.repeatDegree = scaleSize;
  kbm.mapping.resize(static_cast<std::size_t>(std::max(0, scaleSize)));
  for (int i = 0; i < scaleSize; ++i)
    kbm.mapping[static_cast<std::size_t>(i)] = i;
  return kbm;
}

// Combine a scale and a mapping into per-note deviations from 12-TET.
[[nodiscard]] inline TuningTable computeTable(
    const std::vector<double> &scaleCents, const KeyboardMapping &kbm,
    const std::string &name) {
  TuningTable table;
  table.name = name;
  if (scaleCents.empty())
    return table;

  const int scaleSize = static_cast<int>(scaleCents.size());
  table.stepsPerOctave = scaleSize;

  // Degree 0 is the implicit unison; the file's entries are degrees 1..N.
  std::vector<double> degreeCents(static_cast<std::size_t>(scaleSize) + 1u);
  degreeCents[0] = 0.0;
  for (int i = 0; i < scaleSize; ++i)
    degreeCents[static_cast<std::size_t>(i) + 1u] =
        scaleCents[static_cast<std::size_t>(i)];

  // The last entry is the PERIOD -- 2/1 for an octave scale, but 3/1 for
  // Bohlen-Pierce and slightly wide for a stretched tuning.
  const double periodCents = scaleCents[static_cast<std::size_t>(scaleSize - 1)];
  const int mapSize = kbm.mapSize > 0 ? kbm.mapSize : scaleSize;

  const auto centsFromZero = [&](int midiNote) -> double {
    const int keyRelative = midiNote - kbm.middleNote;
    // Floor division: C++ truncates toward zero, which is wrong below middle.
    int period = 0;
    int mapPos = 0;
    if (keyRelative >= 0) {
      period = keyRelative / mapSize;
      mapPos = keyRelative % mapSize;
    } else {
      period = (keyRelative - mapSize + 1) / mapSize;
      mapPos = keyRelative - period * mapSize;
    }
    if (mapPos < 0 || mapPos >= static_cast<int>(kbm.mapping.size()))
      return 0.0;
    const int degree = kbm.mapping[static_cast<std::size_t>(mapPos)];
    if (degree < 0)
      return std::numeric_limits<double>::quiet_NaN();  // unmapped key
    const int clamped = std::clamp(degree, 0, scaleSize);
    return degreeCents[static_cast<std::size_t>(clamped)] +
           static_cast<double>(period) * periodCents;
  };

  const double refCents = centsFromZero(kbm.refNote);
  const bool refUnmapped = std::isnan(refCents);

  for (int n = 0; n < 128; ++n) {
    const double cents = centsFromZero(n);
    if (refUnmapped || std::isnan(cents)) {
      table.centsDeviation[static_cast<std::size_t>(n)] = 0.0f;
      continue;
    }
    const double tuned = kbm.refFreq * std::pow(2.0, (cents - refCents) / 1200.0);
    const double twelveTet = 440.0 * std::pow(2.0, (n - 69) / 12.0);
    table.centsDeviation[static_cast<std::size_t>(n)] =
        static_cast<float>(1200.0 * std::log2(tuned / twelveTet));
  }
  return table;
}

// The whole job, from text. Pass empty .kbm text for the default mapping.
[[nodiscard]] inline TuningTable parseTuning(std::string_view sclText,
                                             std::string_view kbmText = {},
                                             std::string fallbackName = {}) {
  std::string name;
  const std::vector<double> scale = parseScl(sclText, name);
  if (name.empty())
    name = std::move(fallbackName);
  const KeyboardMapping kbm =
      kbmText.empty() ? defaultKbm(static_cast<int>(scale.size()))
                      : parseKbm(kbmText);
  return computeTable(scale, kbm, name);
}

}  // namespace chalkwalk::music
