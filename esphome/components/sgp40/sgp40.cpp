#include "esphome/core/log.h"
#include "sgp40.h"

namespace esphome {
namespace sgp40 {

static const char *TAG = "sgp40";

void SGP40Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SGP40...");

  // Serial Number identification
  if (!this->write_command_(SGP40_CMD_GET_SERIAL_ID)) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }
  uint16_t raw_serial_number[3];

  if (!this->read_data_(raw_serial_number, 3)) {
    this->mark_failed();
    return;
  }
  this->serial_number_ = (uint64_t(raw_serial_number[0]) << 24) | (uint64_t(raw_serial_number[1]) << 16) |
                         (uint64_t(raw_serial_number[2]));
  ESP_LOGD(TAG, "Serial Number: %llu", this->serial_number_);

  // Featureset identification for future use
  if (!this->write_command_(SGP40_CMD_GET_FEATURESET)) {
    ESP_LOGD(TAG, "raw_featureset write_command_ failed");
    this->mark_failed();
    return;
  }
  uint16_t raw_featureset[1];
  if (!this->read_data_(raw_featureset, 1)) {
    ESP_LOGD(TAG, "raw_featureset read_data_ failed");
    this->mark_failed();
    return;
  }

  this->featureset_ = raw_featureset[0];
  if ((this->featureset_ & 0x1FF) != SGP40_FEATURESET) {
    ESP_LOGD(TAG, "Product feature set failed 0x%0X , expecting 0x%0X", uint16_t(this->featureset_ & 0x1FF),
             SGP40_FEATURESET);
    this->mark_failed();
    return;
  }
  ESP_LOGD(TAG, "Product version: 0x%0X", uint16_t(this->featureset_ & 0x1FF));

  VocAlgorithm_init(&this->voc_algorithm_params_);

  this->self_test_();
}

bool SGP40Component::self_test_() {
  uint16_t reply[1];

  ESP_LOGD(TAG, "selfTest start");
  if (!this->write_command_(SGP40_CMD_SELF_TEST)) {
    this->error_code_ = COMMUNICATION_FAILED;
    ESP_LOGD(TAG, "selfTest SGP40_CMD_SELF_TEST failed");
    this->mark_failed();
    return false;
  }
  delay(250);  // NOLINT
  if (!this->read_data_(reply, 1)) {
    ESP_LOGD(TAG, "selfTest read_data_ failed");
    this->mark_failed();
    return false;
  }

  if (reply[0] == 0xD400) {
    ESP_LOGD(TAG, "selfTest completed");
    return true;
  }
  ESP_LOGD(TAG, "selfTest failed");
  this->mark_failed();
  return false;
}

/**
 * @brief Combined the measured gasses, temperature, and humidity
 * to calculate the VOC Index
 *
 * @param temperature The measured temperature in degrees C
 * @param humidity The measured relative humidity in % rH
 * @return int32_t The VOC Index
 */
int32_t SGP40Component::measureVocIndex_() {
  int32_t voc_index;

  uint16_t sraw = measure_raw_();
  ESP_LOGD(TAG, "Got sraw %d", sraw);

  if (sraw == UINT16_MAX)
    return UINT16_MAX;

  VocAlgorithm_process(&voc_algorithm_params_, sraw, &voc_index);
  ESP_LOGD(TAG, "Got voc_index %d", voc_index);
  return voc_index;
}

/**
 * @brief Return the raw gas measurement
 *
 * @param temperature The measured temperature in degrees C
 * @param humidity The measured relative humidity in % rH
 * @return uint16_t The current raw gas measurement
 */
uint16_t SGP40Component::measure_raw_() {
  float humidity = NAN;
  if (this->humidity_sensor_ != nullptr) {
    humidity = this->humidity_sensor_->state;
  }
  if (isnan(humidity) || humidity < 0.0f || humidity > 100.0f) {
    humidity = 50;
  }

  float temperature = NAN;
  if (this->temperature_sensor_ != nullptr) {
    temperature = float(this->temperature_sensor_->state);
  }
  if (isnan(temperature) || temperature < -40.0f || temperature > 85.0f) {
    temperature = 25;
  }

  uint8_t command[8];

  command[0] = 0x26;
  command[1] = 0x0F;

  uint16_t rhticks = (uint16_t)((humidity * 65535) / 100 + 0.5);
  command[2] = rhticks >> 8;
  command[3] = rhticks & 0xFF;
  command[4] = generateCRC(command + 2, 2);
  uint16_t tempticks = (uint16_t)(((temperature + 45) * 65535) / 175);
  command[5] = tempticks >> 8;
  command[6] = tempticks & 0xFF;
  command[7] = generateCRC(command + 5, 2);

  if (!this->write_bytes_raw(command, 8)) {
    this->status_set_warning();
    ESP_LOGD(TAG, "write_bytes_raw error");
    return UINT16_MAX;
  }
  delay(250);  // NOLINT
  uint16_t raw_data[1];

  if (!this->read_data_(raw_data, 1)) {
    this->status_set_warning();
    ESP_LOGD(TAG, "read_data_ error");
    return UINT16_MAX;
  }
  ESP_LOGD(TAG, "measureRaw %d", raw_data[0]);
  return raw_data[0];
}

uint8_t SGP40Component::generateCRC(uint8_t *data, uint8_t datalen) {
  // calculates 8-Bit checksum with given polynomial
  uint8_t crc = SGP40_CRC8_INIT;

  for (uint8_t i = 0; i < datalen; i++) {
    crc ^= data[i];
    for (uint8_t b = 0; b < 8; b++) {
      if (crc & 0x80)
        crc = (crc << 1) ^ SGP40_CRC8_POLYNOMIAL;
      else
        crc <<= 1;
    }
  }
  return crc;
}

void SGP40Component::update() {
  this->set_timeout(50, [this]() {
    uint32_t voc_index = this->measureVocIndex_();
    if (voc_index != UINT16_MAX)
      this->publish_state(voc_index);
  });
}

void SGP40Component::dump_config() {
  ESP_LOGCONFIG(TAG, "SGP40:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    switch (this->error_code_) {
      case COMMUNICATION_FAILED:
        ESP_LOGW(TAG, "Communication failed! Is the sensor connected?");
        break;
      default:
        ESP_LOGW(TAG, "Unknown setup error!");
        break;
    }
  } else {
    ESP_LOGCONFIG(TAG, "  Serial number: %llu", this->serial_number_);
  }
  LOG_UPDATE_INTERVAL(this);

  if (this->humidity_sensor_ != nullptr && this->temperature_sensor_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Compensation:");
    LOG_SENSOR("    ", "Temperature Source:", this->temperature_sensor_);
    LOG_SENSOR("    ", "Humidity Source:", this->humidity_sensor_);
  } else {
    ESP_LOGCONFIG(TAG, "  Compensation: No source configured");
  }
}

bool SGP40Component::write_command_(uint16_t command) {
  // Warning ugly, trick the I2Ccomponent base by setting register to the first 8 bit.
  return this->write_byte(command >> 8, command & 0xFF);
}

uint8_t SGP40Component::sht_crc_(uint8_t data1, uint8_t data2) {
  uint8_t bit;
  uint8_t crc = 0xFF;

  crc ^= data1;
  for (bit = 8; bit > 0; --bit) {
    if (crc & 0x80)
      crc = (crc << 1) ^ 0x131;
    else
      crc = (crc << 1);
  }

  crc ^= data2;
  for (bit = 8; bit > 0; --bit) {
    if (crc & 0x80)
      crc = (crc << 1) ^ 0x131;
    else
      crc = (crc << 1);
  }

  return crc;
}

bool SGP40Component::read_data_(uint16_t *data, uint8_t len) {
  const uint8_t num_bytes = len * 3;
  auto *buf = new uint8_t[num_bytes];

  if (!this->parent_->raw_receive(this->address_, buf, num_bytes)) {
    delete[](buf);
    return false;
  }

  for (uint8_t i = 0; i < len; i++) {
    const uint8_t j = 3 * i;
    uint8_t crc = sht_crc_(buf[j], buf[j + 1]);
    if (crc != buf[j + 2]) {
      ESP_LOGE(TAG, "CRC8 Checksum invalid! 0x%02X != 0x%02X", buf[j + 2], crc);
      delete[](buf);
      return false;
    }
    data[i] = (buf[j] << 8) | buf[j + 1];
  }

  delete[](buf);
  return true;
}

}  // namespace sgp40
}  // namespace esphome
