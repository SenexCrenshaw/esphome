#pragma once

#include "component.h"
#include "helpers.h"

namespace esphome {

inline static uint8_t esp_scale8(uint8_t i, uint8_t scale) { return (uint16_t(i) * (1 + uint16_t(scale))) / 256; }
inline static uint8_t esp_scale(uint8_t i, uint8_t scale, uint8_t max_value) { return (max_value * i / scale); }

struct Color {
  union {
    struct {
      union {
        uint8_t r;
        uint8_t red;
      };
      union {
        uint8_t g;
        uint8_t green;
      };
      union {
        uint8_t b;
        uint8_t blue;
      };
      union {
        uint8_t w;
        uint8_t white;
      };
    };
    uint8_t raw[4];
    uint32_t raw_32;
  };
  inline Color() ALWAYS_INLINE : r(0), g(0), b(0), w(0) {}  // NOLINT
  inline Color(float red, float green, float blue) ALWAYS_INLINE : r(uint8_t(red * 255)),
                                                                   g(uint8_t(green * 255)),
                                                                   b(uint8_t(blue * 255)),
                                                                   w(0) {}
  inline Color(float red, float green, float blue, float white) ALWAYS_INLINE : r(uint8_t(red * 255)),
                                                                                g(uint8_t(green * 255)),
                                                                                b(uint8_t(blue * 255)),
                                                                                w(uint8_t(white * 255)) {}
  inline Color(uint32_t colorcode) ALWAYS_INLINE : r((colorcode >> 16) & 0xFF),
                                                   g((colorcode >> 8) & 0xFF),
                                                   b((colorcode >> 0) & 0xFF),
                                                   w((colorcode >> 24) & 0xFF) {}
  inline bool is_on() ALWAYS_INLINE { return this->raw_32 != 0; }
  inline Color &operator=(const Color &rhs) ALWAYS_INLINE {
    this->r = rhs.r;
    this->g = rhs.g;
    this->b = rhs.b;
    this->w = rhs.w;
    return *this;
  }
  inline Color &operator=(uint32_t colorcode) ALWAYS_INLINE {
    this->w = (colorcode >> 24) & 0xFF;
    this->r = (colorcode >> 16) & 0xFF;
    this->g = (colorcode >> 8) & 0xFF;
    this->b = (colorcode >> 0) & 0xFF;
    return *this;
  }
  inline uint8_t &operator[](uint8_t x) ALWAYS_INLINE { return this->raw[x]; }
  inline Color operator*(uint8_t scale) const ALWAYS_INLINE {
    return Color(esp_scale8(this->red, scale), esp_scale8(this->green, scale), esp_scale8(this->blue, scale),
                 esp_scale8(this->white, scale));
  }
  inline Color &operator*=(uint8_t scale) ALWAYS_INLINE {
    this->red = esp_scale8(this->red, scale);
    this->green = esp_scale8(this->green, scale);
    this->blue = esp_scale8(this->blue, scale);
    this->white = esp_scale8(this->white, scale);
    return *this;
  }
  inline Color operator*(const Color &scale) const ALWAYS_INLINE {
    return Color(esp_scale8(this->red, scale.red), esp_scale8(this->green, scale.green),
                 esp_scale8(this->blue, scale.blue), esp_scale8(this->white, scale.white));
  }
  inline Color &operator*=(const Color &scale) ALWAYS_INLINE {
    this->red = esp_scale8(this->red, scale.red);
    this->green = esp_scale8(this->green, scale.green);
    this->blue = esp_scale8(this->blue, scale.blue);
    this->white = esp_scale8(this->white, scale.white);
    return *this;
  }
  inline Color operator+(const Color &add) const ALWAYS_INLINE {
    Color ret;
    if (uint8_t(add.r + this->r) < this->r)
      ret.r = 255;
    else
      ret.r = this->r + add.r;
    if (uint8_t(add.g + this->g) < this->g)
      ret.g = 255;
    else
      ret.g = this->g + add.g;
    if (uint8_t(add.b + this->b) < this->b)
      ret.b = 255;
    else
      ret.b = this->b + add.b;
    if (uint8_t(add.w + this->w) < this->w)
      ret.w = 255;
    else
      ret.w = this->w + add.w;
    return ret;
  }
  inline Color &operator+=(const Color &add) ALWAYS_INLINE { return *this = (*this) + add; }
  inline Color operator+(uint8_t add) const ALWAYS_INLINE { return (*this) + Color(add, add, add, add); }
  inline Color &operator+=(uint8_t add) ALWAYS_INLINE { return *this = (*this) + add; }
  inline Color operator-(const Color &subtract) const ALWAYS_INLINE {
    Color ret;
    if (subtract.r > this->r)
      ret.r = 0;
    else
      ret.r = this->r - subtract.r;
    if (subtract.g > this->g)
      ret.g = 0;
    else
      ret.g = this->g - subtract.g;
    if (subtract.b > this->b)
      ret.b = 0;
    else
      ret.b = this->b - subtract.b;
    if (subtract.w > this->w)
      ret.w = 0;
    else
      ret.w = this->w - subtract.w;
    return ret;
  }
  inline Color &operator-=(const Color &subtract) ALWAYS_INLINE { return *this = (*this) - subtract; }
  inline Color operator-(uint8_t subtract) const ALWAYS_INLINE {
    return (*this) - Color(subtract, subtract, subtract, subtract);
  }
  inline Color &operator-=(uint8_t subtract) ALWAYS_INLINE { return *this = (*this) - subtract; }
  static Color random_color() {
    float r = float(random_uint32()) / float(UINT32_MAX);
    float g = float(random_uint32()) / float(UINT32_MAX);
    float b = float(random_uint32()) / float(UINT32_MAX);
    float w = float(random_uint32()) / float(UINT32_MAX);
    return Color(r, g, b, w);
  }
  Color fade_to_white(uint8_t amnt) { return Color(1, 1, 1, 1) - (*this * amnt); }
  Color fade_to_black(uint8_t amnt) { return *this * amnt; }
  Color lighten(uint8_t delta) { return *this + delta; }
  Color darken(uint8_t delta) { return *this - delta; }

  uint8_t to_rgb_332() const {
    uint8_t color332 =
        (esp_scale8(this->red, 7) << 5) | (esp_scale8(this->green, 7) << 2) | (esp_scale8(this->blue, 3) << 0);
    return color332;
  }
  uint8_t to_bgr_332() const {
    uint8_t color332 =
        (esp_scale8(this->blue, 3) << 6) | (esp_scale8(this->green, 7) << 3) | (esp_scale8(this->red, 7) << 0);
    return color332;
  }
  static uint32_t rgb_332to_rgb_565(uint8_t rgb332) {
    uint16_t red, green, blue;

    red = (rgb332 & 0xe0) >> 5;  // rgb332 3 red bits now right justified
    red = esp_scale(red, 7, 31);
    red = red << 11;  // red bits now 5 MSB bits

    green = (rgb332 & 0x1c) >> 2;  // rgb332 3 green bits now right justified
    green = esp_scale(green, 7, 63);
    green = green << 5;  // green bits now 6 "middle" bits

    blue = rgb332 & 0x03;  // rgb332 2 blue bits are right justified
    blue = esp_scale(blue, 3, 31);

    return (uint16_t)(red | green | blue);
  }
  static uint32_t bgr_233to_bgr_565(uint8_t bgr233) {
    uint16_t red, green, blue;

    blue = (bgr233 & 0xc0) >> 6;  // bgr233 2 blue bits now right justified
    blue = esp_scale(blue, 3, 31);
    blue = blue << 11;  // blue bits now 5 MSB bits

    green = (bgr233 & 0x38) >> 3;  // bgr233 3 green bits now right justified
    green = esp_scale(green, 7, 63);
    green = green << 5;  // green bits now 6 "middle" bits

    red = bgr233 & 0x07;  // rgb332 3 red bits are right justified
    red = esp_scale(red, 7, 31);

    return (uint16_t)(blue | green | red);
  }
  uint32_t to_rgb_565() const {
    uint32_t color565 =
        (esp_scale8(this->red, 31) << 11) | (esp_scale8(this->green, 63) << 5) | (esp_scale8(this->blue, 31) << 0);
    return color565;
  }
  uint32_t to_bgr_565() const {
    uint32_t color565 =
        (esp_scale8(this->blue, 31) << 11) | (esp_scale8(this->green, 63) << 5) | (esp_scale8(this->red, 31) << 0);
    return color565;
  }
  uint32_t to_grayscale4() const {
    uint32_t gs4 = esp_scale8(this->white, 15);
    return gs4;
  }
};
static const Color COLOR_BLACK(0, 0, 0);
static const Color COLOR_WHITE(1, 1, 1);
};  // namespace esphome
