#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace budget::ui {

class Color {
 public:
  Color() = default;
  Color(uint8_t r, uint8_t g, uint8_t b);

  static Color FromAnsi256(int code);
  int ToAnsi256() const;

  uint8_t r() const { return r_; }
  uint8_t g() const { return g_; }
  uint8_t b() const { return b_; }

  // Relative luminance (sRGB) in [0,1]
  double GetLuminosity() const;

  // Returns black or white depending on contrast.
  Color GetContrastingColor() const;

  // Returns two complementary colors (hue +180).
  std::array<Color, 2> GetComplementaryColors() const;

  // ANSI cube helpers (0-5 per channel)
  std::array<int, 3> ToAnsiCube() const;
  static Color FromAnsiCube(int r, int g, int b);

  // ANSI cube luminosity/contrast/complementary (matches AnsiColorManager
  // logic)
  double GetLuminosityCube() const;
  Color GetContrastingColorCube(int percentage = 100) const;
  Color GetComplementaryColorCube() const;

 private:
  uint8_t r_ = 0;
  uint8_t g_ = 0;
  uint8_t b_ = 0;

  static double srgb_to_linear(double c);
  static Color FromHsl(double h, double s, double l);
  static void ToHsl(const Color& c, double& h, double& s, double& l);
};

}  // namespace budget::ui
