#include "st7735.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace st7735 {

// clang-format on
static const char *TAG = "st7735";

ST7735::ST7735(ST7735Model model, int width, int height, int colstart, int rowstart, boolean usebgr,
               bufferex_base::BufferexBase *bufferex_base) {
  model_ = model;
  this->width_ = width;
  this->height_ = height;
  this->colstart_ = colstart;
  this->rowstart_ = rowstart;
  this->usebgr_ = usebgr;
  this->bufferex_base_ = bufferex_base;
}

void ST7735::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ST7735...");
  this->spi_setup();

  this->dc_pin_->setup();
  this->cs_->setup();

  this->dc_pin_->digital_write(true);
  this->cs_->digital_write(true);

  this->init_reset_();
  delay(100);  // NOLINT

  ESP_LOGD(TAG, "  START");
  dump_config();
  ESP_LOGD(TAG, "  END");

  display_init_(RCMD1);

  if (this->model_ == INITR_GREENTAB) {
    display_init_(RCMD2GREEN);
    colstart_ == 0 ? colstart_ = 2 : colstart_;
    rowstart_ == 0 ? rowstart_ = 1 : rowstart_;
  } else if ((this->model_ == INITR_144GREENTAB) || (this->model_ == INITR_HALLOWING)) {
    height_ == 0 ? height_ = ST7735_TFTHEIGHT_128 : height_;
    width_ == 0 ? width_ = ST7735_TFTWIDTH_128 : width_;
    display_init_(RCMD2GREEN144);
    colstart_ == 0 ? colstart_ = 2 : colstart_;
    rowstart_ == 0 ? rowstart_ = 3 : rowstart_;
  } else if (this->model_ == INITR_MINI_160X80) {
    height_ == 0 ? height_ = ST7735_TFTHEIGHT_160 : height_;
    width_ == 0 ? width_ = ST7735_TFTWIDTH_80 : width_;
    display_init_(RCMD2GREEN160X80);
    colstart_ = 24;
    rowstart_ = 0;  // For default rotation 0
  } else {
    // colstart, rowstart left at default '0' values
    display_init_(RCMD2RED);
  }
  display_init_(RCMD3);

  uint8_t data = 0;
  if (this->model_ != INITR_HALLOWING) {
    uint8_t data = ST77XX_MADCTL_MX | ST77XX_MADCTL_MY;
  }
  if (this->usebgr_) {
    data = data | ST7735_MADCTL_BGR;
  } else {
    data = data | ST77XX_MADCTL_RGB;
  }
  sendcommand_(ST77XX_MADCTL, &data, 1);

  bufferex_base_->init_buffer(this->width_, this->height_);
  this->fill_internal_(COLOR_BLACK);
}

void ST7735::fill_internal_(Color color) {
  this->set_addr_window_(0, 0, this->get_width_internal(), this->get_height_internal());
  this->start_data_();

  auto color565 = color.to_565();
  for (uint32_t i = 0; i < this->bufferex_base_->get_buffer_length(); i++) {
    this->write_byte(color565 >> 8);
    this->write_byte(color565);
  }
  this->end_data_();
}

void ST7735::update() {
  this->do_update_();
  this->display_buffer_();
  this->bufferex_base_->display();
}

int ST7735::get_height_internal() { return height_; }

int ST7735::get_width_internal() { return width_; }

void HOT ST7735::draw_absolute_pixel_internal(int x, int y, Color color) {
  pixel_count_++;
  this->bufferex_base_->set_pixel(x, y, color);
}

void ST7735::init_reset_() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(true);
    delay(1);
    // Trigger Reset
    this->reset_pin_->digital_write(false);
    delay(10);
    // Wake up
    this->reset_pin_->digital_write(true);
  }
}
const char *ST7735::model_str_() {
  switch (this->model_) {
    case INITR_GREENTAB:
      return "ST7735 GREENTAB";
    case INITR_REDTAB:
      return "ST7735 REDTAB";
    case INITR_BLACKTAB:
      return "ST7735 BLACKTAB";
    case INITR_MINI_160X80:
      return "ST7735 MINI160x80";
    default:
      return "Unknown";
  }
}

void ST7735::display_init_(const uint8_t *addr) {
  uint8_t num_commands, cmd, num_args;
  uint16_t ms;

  num_commands = pgm_read_byte(addr++);  // Number of commands to follow
  while (num_commands--) {               // For each command...
    cmd = pgm_read_byte(addr++);         // Read command
    num_args = pgm_read_byte(addr++);    // Number of args to follow
    ms = num_args & ST_CMD_DELAY;        // If hibit set, delay follows args
    num_args &= ~ST_CMD_DELAY;           // Mask out delay bit
    this->sendcommand_(cmd, addr, num_args);
    addr += num_args;

    if (ms) {
      ms = pgm_read_byte(addr++);  // Read post-command delay time (ms)
      if (ms == 255)
        ms = 500;  // If 255, delay for 500 ms
      delay(ms);
    }
  }
}

void ST7735::dump_config() {
  LOG_DISPLAY("", "ST7735", this);
  ESP_LOGCONFIG(TAG, "  Model: %s", this->model_str_());
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  ESP_LOGD(TAG, "  Buffer Length: %zu", this->bufferex_base_->get_buffer_length());
  ESP_LOGD(TAG, "  Buffer Size: %zu", this->bufferex_base_->get_buffer_size());
  ESP_LOGD(TAG, "  Height: %d", this->height_);
  ESP_LOGD(TAG, "  Width: %d", this->width_);
  ESP_LOGD(TAG, "  ColStart: %d", this->colstart_);
  ESP_LOGD(TAG, "  RowStart: %d", this->rowstart_);
  LOG_UPDATE_INTERVAL(this);
}

void ST7735::fill(Color color) { this->bufferex_base_->fill_buffer(color); }

void HOT ST7735::writecommand_(uint8_t value) {
  this->enable();
  this->dc_pin_->digital_write(false);
  this->write_byte(value);
  this->dc_pin_->digital_write(true);
  this->disable();
}

void HOT ST7735::writedata_(uint8_t value) {
  this->dc_pin_->digital_write(true);
  this->enable();
  this->write_byte(value);
  this->disable();
}

void HOT ST7735::sendcommand_(uint8_t cmd, const uint8_t *data_bytes, uint8_t num_data_bytes) {
  this->writecommand_(cmd);
  this->senddata_(data_bytes, num_data_bytes);
}

void HOT ST7735::senddata_(const uint8_t *data_bytes, uint8_t num_data_bytes) {
  this->dc_pin_->digital_write(true);  // pull DC high to indicate data
  this->cs_->digital_write(false);
  this->enable();
  for (uint8_t i = 0; i < num_data_bytes; i++) {
    this->transfer_byte(pgm_read_byte(data_bytes++));  // write byte - SPI library
  }
  this->cs_->digital_write(true);
  this->disable();
}

void HOT ST7735::display_buffer() {
  ESP_LOGD(TAG, "Asked to write %d pixels", this->pixel_count_);
  const int w = this->bufferex_base_->x_high_ - this->bufferex_base_->x_low_ + 1;
  const int h = this->bufferex_base_->y_high_ - this->bufferex_base_->y_low_ + 1;
  const uint32_t start_pos = ((this->bufferex_base_->y_low_ * this->width_) + this->bufferex_base_->x_low_);

  set_addr_window_(this->bufferex_base_->x_low_ + colstart_, this->bufferex_base_->y_low_ + rowstart_, w, h);

  this->start_data_();
  for (uint16_t row = 0; row < h; row++) {
    for (uint16_t col = 0; col < w; col++) {
      uint32_t pos = start_pos + (row * width_) + col;

      auto color = this->bufferex_base_->get_pixel_to_565(pos);
      this->write_byte(color >> 8);
      this->write_byte(color);
    }
  }
  this->end_data_();
  this->pixel_count_ = 0;
}

void ST7735::start_data_() {
  this->dc_pin_->digital_write(true);
  this->enable();
}
void ST7735::end_data_() { this->disable(); }

void ST7735::set_addr_window_(uint16_t x1, uint16_t y1, uint16_t w, uint16_t h) {
  uint16_t x2 = (x1 + w - 1), y2 = (y1 + h - 1);

  this->enable();

  // set column(x) address
  this->dc_pin_->digital_write(false);
  this->write_byte(ST77XX_CASET);
  this->dc_pin_->digital_write(true);
  this->spi_master_write_addr_(x1, x2);

  // set Page(y) address
  this->dc_pin_->digital_write(false);
  this->write_byte(ST77XX_RASET);
  this->dc_pin_->digital_write(true);
  this->spi_master_write_addr_(y1, y2);

  //  Memory Write
  this->dc_pin_->digital_write(false);
  this->write_byte(ST77XX_RAMWR);
  this->dc_pin_->digital_write(true);

  // if (this->bufferex_base_->get_pixel_bit_size() == 16) {  // 16bit just write. I am a 565 display
  //   this->write_array(this->bufferex_base_->buffer_, this->bufferex_base_->get_buffer_length());
  // } else {
  for (int y = 0; y < this->get_height_internal(); ++y) {
    for (int x = 0; x < this->get_width_internal(); ++x) {
      auto color = this->bufferex_base_->get_pixel_to_565(x, y);
      this->write_byte((color >> 8) & 0xff);
      this->write_byte(color & 0xff);
    }
  }
  this->disable();
}

void ST7735::spi_master_write_addr_(uint16_t addr1, uint16_t addr2) {
  static uint8_t BYTE[4];
  BYTE[0] = (addr1 >> 8) & 0xFF;
  BYTE[1] = addr1 & 0xFF;
  BYTE[2] = (addr2 >> 8) & 0xFF;
  BYTE[3] = addr2 & 0xFF;

  this->dc_pin_->digital_write(true);
  this->write_array(BYTE, 4);
}

}  // namespace st7735
}  // namespace esphome
