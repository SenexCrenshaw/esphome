#include "bufferex_indexed8.h"

namespace esphome {
namespace display {
static const char *TAG = "bufferex_indexed8";

void BufferexIndexed8::init_buffer(int width, int height) {
  this->width_ = width;
  this->height_ = height;

#ifdef ARDUINO_ARCH_ESP32
  uint8_t *psrambuffer = (uint8_t *) malloc(1);  // NOLINT

  if (psrambuffer == nullptr) {
    ESP_LOGW(TAG, "PSRAM is NOT supported");
  } else {
    ESP_LOGW(TAG, "PSRAM is supported");
    ESP_LOGW(TAG, "Total heap: %d", ESP.getHeapSize());
    ESP_LOGW(TAG, "Free heap: %d", ESP.getFreeHeap());
    ESP_LOGW(TAG, "Total PSRAM: %d", ESP.getPsramSize());
    ESP_LOGW(TAG, "Free PSRAM: %d", ESP.getFreeHeap());
  }
#endif
  this->buffer_ = new uint8_t[this->get_buffer_length()];
  if (this->buffer_ == nullptr) {
    ESP_LOGE(TAG, "Could not allocate buffer for display!");
    return;
  }
  memset(this->buffer_, 0x00, this->get_buffer_size());
}

void BufferexIndexed8::fill_buffer(Color color) {
  display::BufferexBase::fill_buffer(color);

  ESP_LOGD(TAG, "fill_buffer %d", color.b);
  for (uint16_t x = 0; x < this->height_; ++x) {
    for (uint16_t y = 0; y < this->width_; y++) {
      this->set_buffer(x, y, color.b);
    }
  }
}

void HOT BufferexIndexed8::set_buffer(int x, int y, Color color) {
  uint32_t pos = (x + y * this->width_);
  bool debug = false;

  uint8_t index = 0;
  for (int i = 0; i < colors_.size(); i++) {
    if (colors_[i].r == color.r && colors_[i].g == color.g && colors_[i].b == color.b) {
      index = i;
      break;
    }
  }

  const uint32_t pixel_bit_start = pos * this->pixel_storage_size_;
  const uint32_t pixel_bit_end = pixel_bit_start + this->pixel_storage_size_;

  const uint32_t byte_location_start = pixel_bit_start / 8;
  const uint32_t byte_location_end = pixel_bit_end / 8;

  const uint8_t byte_offset_start = pixel_bit_start % 8;

  uint8_t index_byte_start = this->buffer_[byte_location_start];
  uint8_t mask = ((1 << this->pixel_storage_size_) - 1) << byte_offset_start;

  index_byte_start = (index_byte_start & ~mask) | ((index << byte_offset_start) & mask);
  this->buffer_[byte_location_start] = index_byte_start;

  if (byte_location_start == byte_location_end) {
    // Pixel starts and ends in the same byte, so we're done.
    return;
  }

  const uint8_t byte_offset_end = pixel_bit_end % 8;

  uint8_t index_byte_end = this->buffer_[byte_location_end];
  mask = (((uint8_t) 1 << this->pixel_storage_size_) - 1) >> (this->pixel_storage_size_ - byte_offset_end);

  index_byte_end = (index_byte_end & ~mask) | ((index >> (this->pixel_storage_size_ - byte_offset_end)) & mask);

  this->buffer_[byte_location_end] = index_byte_end;
}

uint8_t HOT BufferexIndexed8::get_index_value_(int x, int y) { return this->get_index_value_((x + y * this->width_)); }

uint8_t HOT BufferexIndexed8::get_index_value_(uint32_t pos) {
  const uint32_t pixel_bit_start = pos * this->pixel_storage_size_;
  const uint32_t pixel_bit_end = pixel_bit_start + this->pixel_storage_size_;

  const uint32_t byte_location_start = pixel_bit_start / 8;
  const uint32_t byte_location_end = pixel_bit_end / 8;

  uint8_t index_byte_start = this->buffer_[byte_location_start];
  const uint8_t byte_offset_start = pixel_bit_start % 8;

  uint8_t mask = (1 << this->pixel_storage_size_) - 1;

  index_byte_start = (index_byte_start >> byte_offset_start);

  if (byte_location_start == byte_location_end) {
    // Pixel starts and ends in the same byte, so we're done.
    return index_byte_start & mask;
  }

  const uint8_t byte_offset_end = pixel_bit_end % 8;

  uint8_t end_mask = mask >> (this->pixel_storage_size_ - byte_offset_end);

  uint8_t index_byte_end = this->buffer_[byte_location_end];

  index_byte_end = (index_byte_end & end_mask) << (this->pixel_storage_size_ - byte_offset_end);

  index_byte_end = index_byte_end | index_byte_start;

  return index_byte_end & mask;
}

uint16_t BufferexIndexed8::get_pixel_to_565(int x, int y) {
  uint8_t value = this->get_index_value_(x, y);

  if (value > this->index_size_)
    value = 0;

  return this->colors_[value].to_565(this->driver_right_bit_aligned_);
}

uint16_t BufferexIndexed8::get_pixel_to_565(uint32_t pos) {
  uint8_t value = this->get_index_value_(pos);

  if (value > this->index_size_)
    value = 0;

  return this->colors_[value].to_565(this->driver_right_bit_aligned_);
}

uint32_t HOT BufferexIndexed8::get_pixel_to_666(int x, int y) {
  uint8_t value = this->get_index_value_(x, y);

  if (x == 0)
    ESP_LOGD(TAG, "Got value %d from x %d, y %d", value, x, y);

  if (value > this->index_size_)
    value = 0;

  return this->colors_[value].to_666(this->driver_right_bit_aligned_);
}

uint32_t HOT BufferexIndexed8::get_pixel_to_666(uint32_t pos) {
  uint8_t value = this->get_index_value_(pos);

  if (value > this->index_size_)
    value = 0;

  return this->colors_[value].to_666(this->driver_right_bit_aligned_);
}

size_t BufferexIndexed8::get_buffer_length() {  // How many unint8_t bytes does the buffer need
  if (this->get_buffer_length_ != 0)
    return this->get_buffer_length_;

  auto total_pixels = size_t(this->width_) * size_t(this->height_);

  if (this->index_size_ <= 1) {
    this->pixel_storage_size_ = 1;
  } else {
    this->pixel_storage_size_ = std::ceil(std::log(this->index_size_) / std::log(2));
  }

  auto screensize = total_pixels * pixel_storage_size_;

  auto bufflength = (screensize % 8) ? screensize / 8 + 1 : screensize / 8;

  ESP_LOGD(TAG, "Pixel index size: %hhu", this->index_size_);
  ESP_LOGD(TAG, "Total Pixels: %zu", total_pixels);
  ESP_LOGD(TAG, "Pixel Storage Size: %d", this->pixel_storage_size_);
  ESP_LOGD(TAG, "Buffer length %zu", bufflength);

  this->get_buffer_length_ = bufflength;

  return bufflength;
}

size_t BufferexIndexed8::get_buffer_size() { return this->get_buffer_length(); }

}  // namespace display
}  // namespace esphome
