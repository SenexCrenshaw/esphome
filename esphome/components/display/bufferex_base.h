#pragma once
#include "esphome/core/color.h"
#include "esphome/core/log.h"
namespace esphome {
namespace display {

enum BufferType : uint8_t {
  BUFFER_TYPE_1BIT = 0,
  BUFFER_TYPE_332 = 1,
  BUFFER_TYPE_565 = 2,
  BUFFER_TYPE_666 = 3,
  BUFFER_TYPE_INDEXED = 4,
};
static const std::string BUFFER_TYPE_STRINGS[] = {"1BIT", "332", "565", "666", "888"};

class BufferexBase {
 public:
  virtual void init_buffer(int width, int height) = 0;
  virtual void set_buffer(int x, int y, Color color) = 0;
  virtual void set_buffer(int x, int y, uint8_t index);

  // 565
  virtual uint16_t get_pixel_to_565(int x, int y) {
    const uint16_t pos = get_pixel_buffer_position_(x, y);
    return this->get_pixel_to_565(pos);
  }
  virtual uint16_t get_pixel_to_565(uint16_t pos) = 0;

  // 666
  virtual uint32_t get_pixel_to_666(int x, int y) {
    const uint16_t pos = get_pixel_buffer_position_(x, y);
    return this->get_pixel_to_666(pos);
  };
  virtual uint32_t get_pixel_to_666(uint16_t pos) = 0;

  virtual size_t get_buffer_size() = 0;
  virtual size_t get_buffer_length();
  virtual display::BufferType get_buffer_type() = 0;
  virtual uint8_t get_pixel_storage_size() = 0;
  virtual void fill_buffer(Color color);

  void display();
  void set_pixel(int x, int y, Color color);
  void set_driver_right_bit_aligned(bool driver_right_bit_aligned) {
    this->driver_right_bit_aligned_ = driver_right_bit_aligned;
  }

  uint16_t x_low_ = 0;
  uint16_t y_low_ = 0;
  uint16_t x_high_ = 0;
  uint16_t y_high_ = 0;

 protected:
  int16_t width_ = 0, height_ = 0;
  bool driver_right_bit_aligned_ = false;
  uint16_t get_pixel_buffer_position_(int x, int y) { return (x + y * width_); }

};  // class BufferexBase
}  // namespace display
}  // namespace esphome
