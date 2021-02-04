#include "bufferex_565.h"

namespace esphome {
namespace display {
static const char *TAG = "bufferex_565";

void Bufferex565::init_buffer(int width, int height) {
  this->width_ = width;
  this->height_ = height;

  this->buffer_ = new uint16_t[this->get_buffer_length()];
  if (this->buffer_ == nullptr) {
    ESP_LOGE(TAG, "Could not allocate buffer for display!");
    return;
  }
  memset(this->buffer_, 0x00, this->get_buffer_size());
}

void Bufferex565::fill_buffer(Color color) {
  display::BufferexBase::fill_buffer(color);

  auto color565 = color.to_565();
  ESP_LOGD(TAG, "fill_buffer color: %d", color565);
  memset(this->buffer_, color565, this->get_buffer_size());
}

size_t Bufferex565::get_buffer_size() { return this->get_buffer_length() * 2; }

void HOT Bufferex565::set_buffer(int x, int y, Color color) {
  const uint16_t color565 = color.to_565();
  uint16_t pos = get_pixel_buffer_position_(x, y);
  this->buffer_[pos] = color565;
}

// 565
uint16_t Bufferex565::get_pixel_to_565(uint16_t pos) { return this->buffer_[pos]; }

// 666
uint32_t Bufferex565::get_pixel_to_666(uint16_t pos) {
  return Color(this->buffer_[pos], Color::ColorOrder::COLOR_ORDER_RGB, Color::ColorBitness::COLOR_BITNESS_565, true)
      .to_666();
}

}  // namespace display
}  // namespace esphome
