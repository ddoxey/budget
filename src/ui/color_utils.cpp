#include "color_utils.h"

#include <algorithm>
#include <cmath>

namespace budget::ui {

namespace {

constexpr std::array<std::array<uint8_t, 3>, 16> kAnsi16 = {{
    {0, 0, 0},        // 0 black
    {128, 0, 0},      // 1 red
    {0, 128, 0},      // 2 green
    {128, 128, 0},    // 3 yellow
    {0, 0, 128},      // 4 blue
    {128, 0, 128},    // 5 magenta
    {0, 128, 128},    // 6 cyan
    {192, 192, 192},  // 7 white (light gray)
    {128, 128, 128},  // 8 bright black (dark gray)
    {255, 0, 0},      // 9 bright red
    {0, 255, 0},      // 10 bright green
    {255, 255, 0},    // 11 bright yellow
    {0, 0, 255},      // 12 bright blue
    {255, 0, 255},    // 13 bright magenta
    {0, 255, 255},    // 14 bright cyan
    {255, 255, 255}   // 15 bright white
}};

uint8_t cube_level(int idx) {
  static const uint8_t levels[6] = {0, 95, 135, 175, 215, 255};
  return levels[idx];
}

int cube_index(uint8_t value) {
  static const uint8_t levels[6] = {0, 95, 135, 175, 215, 255};
  int best = 0;
  int best_dist = std::abs(static_cast<int>(value) - levels[0]);
  for (int i = 1; i < 6; ++i) {
    int d = std::abs(static_cast<int>(value) - levels[i]);
    if (d < best_dist) {
      best_dist = d;
      best = i;
    }
  }
  return best;
}

}  // namespace

Color::Color(uint8_t r, uint8_t g, uint8_t b) : r_(r), g_(g), b_(b) {}

Color Color::FromAnsi256(int code) {
  if (code < 0) {
    code = 0;
  }
  if (code > 255) {
    code = 255;
  }
  if (code < 16) {
    return Color(kAnsi16[code][0], kAnsi16[code][1], kAnsi16[code][2]);
  }
  if (code >= 232) {
    uint8_t v = static_cast<uint8_t>(8 + (code - 232) * 10);
    return Color(v, v, v);
  }
  int idx = code - 16;
  int r = idx / 36;
  int g = (idx / 6) % 6;
  int b = idx % 6;
  return Color(cube_level(r), cube_level(g), cube_level(b));
}

int Color::ToAnsi256() const {
  // Find nearest in 6x6x6 cube + grayscale.
  auto dist = [](int r1, int g1, int b1, int r2, int g2, int b2) {
    int dr = r1 - r2;
    int dg = g1 - g2;
    int db = b1 - b2;
    return dr * dr + dg * dg + db * db;
  };

  int best_code = 0;
  int best_dist = dist(r_, g_, b_, kAnsi16[0][0], kAnsi16[0][1], kAnsi16[0][2]);

  for (int i = 0; i < 16; ++i) {
    int d = dist(r_, g_, b_, kAnsi16[i][0], kAnsi16[i][1], kAnsi16[i][2]);
    if (d < best_dist) {
      best_dist = d;
      best_code = i;
    }
  }

  for (int r = 0; r < 6; ++r) {
    for (int g = 0; g < 6; ++g) {
      for (int b = 0; b < 6; ++b) {
        int code = 16 + r * 36 + g * 6 + b;
        int d = dist(r_, g_, b_, cube_level(r), cube_level(g), cube_level(b));
        if (d < best_dist) {
          best_dist = d;
          best_code = code;
        }
      }
    }
  }

  for (int i = 0; i < 24; ++i) {
    int code = 232 + i;
    uint8_t v = static_cast<uint8_t>(8 + i * 10);
    int d = dist(r_, g_, b_, v, v, v);
    if (d < best_dist) {
      best_dist = d;
      best_code = code;
    }
  }

  return best_code;
}

std::array<int, 3> Color::ToAnsiCube() const {
  return {cube_index(r_), cube_index(g_), cube_index(b_)};
}

Color Color::FromAnsiCube(int r, int g, int b) {
  r = std::clamp(r, 0, 5);
  g = std::clamp(g, 0, 5);
  b = std::clamp(b, 0, 5);
  return Color(cube_level(r), cube_level(g), cube_level(b));
}

double Color::srgb_to_linear(double c) {
  if (c <= 0.04045) {
    return c / 12.92;
  }
  return std::pow((c + 0.055) / 1.055, 2.4);
}

double Color::GetLuminosity() const {
  double r = srgb_to_linear(r_ / 255.0);
  double g = srgb_to_linear(g_ / 255.0);
  double b = srgb_to_linear(b_ / 255.0);
  return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}

Color Color::GetContrastingColor() const {
  return GetLuminosity() > 0.5 ? Color(0, 0, 0) : Color(255, 255, 255);
}

double Color::GetLuminosityCube() const {
  auto cube = ToAnsiCube();
  return 0.2126 * cube[0] + 0.7152 * cube[1] + 0.0722 * cube[2];
}

Color Color::GetContrastingColorCube(int percentage) const {
  auto cube = ToAnsiCube();
  double r = cube[0];
  double g = cube[1];
  double b = cube[2];
  double luminosity = GetLuminosityCube();
  if (luminosity >= 2.5) {
    r = std::max(0.0, r - r * (percentage / 100.0));
    g = std::max(0.0, g - g * (percentage / 100.0));
    b = std::max(0.0, b - b * (percentage / 100.0));
  } else {
    r = std::min(5.0, r + (5.0 - r) * (percentage / 100.0));
    g = std::min(5.0, g + (5.0 - g) * (percentage / 100.0));
    b = std::min(5.0, b + (5.0 - b) * (percentage / 100.0));
  }
  return FromAnsiCube(static_cast<int>(r), static_cast<int>(g),
                      static_cast<int>(b));
}

Color Color::GetComplementaryColorCube() const {
  auto cube = ToAnsiCube();
  return FromAnsiCube(5 - cube[0], 5 - cube[1], 5 - cube[2]);
}

void Color::ToHsl(const Color& c, double& h, double& s, double& l) {
  double r = c.r_ / 255.0;
  double g = c.g_ / 255.0;
  double b = c.b_ / 255.0;

  double maxv = std::max({r, g, b});
  double minv = std::min({r, g, b});
  l = (maxv + minv) / 2.0;

  if (maxv == minv) {
    h = 0.0;
    s = 0.0;
    return;
  }

  double d = maxv - minv;
  s = l > 0.5 ? d / (2.0 - maxv - minv) : d / (maxv + minv);

  if (maxv == r) {
    h = (g - b) / d + (g < b ? 6.0 : 0.0);
  } else if (maxv == g) {
    h = (b - r) / d + 2.0;
  } else {
    h = (r - g) / d + 4.0;
  }
  h *= 60.0;
}

Color Color::FromHsl(double h, double s, double l) {
  auto hue_to_rgb = [](double p, double q, double t) {
    if (t < 0.0) t += 1.0;
    if (t > 1.0) t -= 1.0;
    if (t < 1.0 / 6.0) return p + (q - p) * 6.0 * t;
    if (t < 1.0 / 2.0) return q;
    if (t < 2.0 / 3.0) return p + (q - p) * (2.0 / 3.0 - t) * 6.0;
    return p;
  };

  double r, g, b;
  if (s == 0.0) {
    r = g = b = l;
  } else {
    double q = l < 0.5 ? l * (1.0 + s) : l + s - l * s;
    double p = 2.0 * l - q;
    r = hue_to_rgb(p, q, (h / 360.0) + 1.0 / 3.0);
    g = hue_to_rgb(p, q, h / 360.0);
    b = hue_to_rgb(p, q, (h / 360.0) - 1.0 / 3.0);
  }

  return Color(static_cast<uint8_t>(std::round(r * 255.0)),
               static_cast<uint8_t>(std::round(g * 255.0)),
               static_cast<uint8_t>(std::round(b * 255.0)));
}

std::array<Color, 2> Color::GetComplementaryColors() const {
  double h, s, l;
  ToHsl(*this, h, s, l);
  double h1 = std::fmod(h + 180.0, 360.0);
  return {FromHsl(h1, s, l), FromHsl(h1, s, l)};
}

}  // namespace budget::ui
