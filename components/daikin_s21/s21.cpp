#include <cinttypes>
#include <numeric>
#include "s21.h"

using namespace esphome;

namespace esphome {
namespace daikin_s21 {

#define STX 2
#define ETX 3
#define ENQ 5
#define ACK 6
#define NAK 21

static const char *const TAG = "daikin_s21";

std::string daikin_climate_mode_to_string(DaikinClimateMode mode) {
  switch (mode) {
    case DaikinClimateMode::Disabled:
      return "Disabled";
    case DaikinClimateMode::Auto:
      return "Auto";
    case DaikinClimateMode::Dry:
      return "Dry";
    case DaikinClimateMode::Cool:
      return "Cool";
    case DaikinClimateMode::Heat:
      return "Heat";
    case DaikinClimateMode::Fan:
      return "Fan";
    default:
      return "UNKNOWN";
  }
}

std::string daikin_fan_mode_to_string(DaikinFanMode mode) {
  switch (mode) {
    case DaikinFanMode::Auto:
      return "Auto";
    case DaikinFanMode::Silent:
      return "Silent";
    case DaikinFanMode::Speed1:
      return "1";
    case DaikinFanMode::Speed2:
      return "2";
    case DaikinFanMode::Speed3:
      return "3";
    case DaikinFanMode::Speed4:
      return "4";
    case DaikinFanMode::Speed5:
      return "5";
    default:
      return "UNKNOWN";
  }
}

int16_t bytes_to_num(uint8_t *bytes, size_t len) {
  // <ones><tens><hundreds><neg/pos>
  int16_t val = 0;
  val = bytes[0] - '0';
  val += (bytes[1] - '0') * 10;
  val += (bytes[2] - '0') * 100;
  if (len > 3 && bytes[3] == '-')
    val *= -1;
  return val;
}

int16_t temp_bytes_to_c10(uint8_t *bytes) { return bytes_to_num(bytes, 4); }

int16_t temp_f9_byte_to_c10(uint8_t *bytes) { return (*bytes / 2 - 64) * 10; }

uint8_t c10_to_setpoint_byte(int16_t setpoint) {
  return (setpoint + 3) / 5 + 28;
}

#define S21_BAUD_RATE 2400
#define S21_STOP_BITS 2
#define S21_DATA_BITS 8
#define S21_PARITY uart::UART_CONFIG_PARITY_EVEN

void DaikinSerial::set_uarts(uart::UARTComponent *tx, uart::UARTComponent *rx) {
  this->tx_uart = tx;
  this->rx_uart = rx;

  for (auto *uart : {this->tx_uart, this->rx_uart}) {
    uart->set_baud_rate(S21_BAUD_RATE);
    uart->set_stop_bits(S21_STOP_BITS);
    uart->set_data_bits(S21_DATA_BITS);
    uart->set_parity(S21_PARITY);
    uart->load_settings();
  }
}

void DaikinS21::dump_config() {
  ESP_LOGCONFIG(TAG, "DaikinS21:");
  ESP_LOGCONFIG(TAG, "  Update interval: %" PRIu32, this->get_update_interval());
}

// Adapated from ESPHome UART debugger
std::string hex_repr(const uint8_t *bytes, size_t len) {
  std::string res;
  char buf[5];
  for (size_t i = 0; i < len; i++) {
    if (i > 0)
      res += ':';
    sprintf(buf, "%02X", bytes[i]);
    res += buf;
  }
  return res;
}

// Adapated from ESPHome UART debugger
std::string str_repr(const uint8_t *bytes, size_t len) {
  std::string res;
  char buf[5];
  for (size_t i = 0; i < len; i++) {
    if (bytes[i] == 7) {
      res += "\\a";
    } else if (bytes[i] == 8) {
      res += "\\b";
    } else if (bytes[i] == 9) {
      res += "\\t";
    } else if (bytes[i] == 10) {
      res += "\\n";
    } else if (bytes[i] == 11) {
      res += "\\v";
    } else if (bytes[i] == 12) {
      res += "\\f";
    } else if (bytes[i] == 13) {
      res += "\\r";
    } else if (bytes[i] == 27) {
      res += "\\e";
    } else if (bytes[i] == 34) {
      res += "\\\"";
    } else if (bytes[i] == 39) {
      res += "\\'";
    } else if (bytes[i] == 92) {
      res += "\\\\";
    } else if (bytes[i] < 32 || bytes[i] > 127) {
      sprintf(buf, "\\x%02X", bytes[i]);
      res += buf;
    } else {
      res += bytes[i];
    }
  }
  return res;
}

DaikinSerial::Result DaikinSerial::handle_rx(const uint8_t byte) {
  Result result = Result::Busy; // default to busy, override when rx phase is over

  switch (comm_state) {
    case CommState::QueryAck:
    case CommState::CommandAck:
      switch (byte) {
        case ACK:
          if (comm_state == CommState::QueryAck) {
            comm_state = CommState::QueryStx;
          } else {
            comm_state = CommState::Cooldown;
            cooldown_length = S21_RESPONSE_TURNAROUND;
            result = Result::Ack;
          }
          break;
        case NAK:
          comm_state = CommState::Cooldown;
          cooldown_length = S21_RESPONSE_TURNAROUND;
          result = Result::Nak;
          break;
        default:
          ESP_LOGW(TAG, "Rx ACK: Unuexpected 0x%02X", byte);
          comm_state = CommState::Cooldown;
          cooldown_length = S21_ERROR_TIMEOUT;
          result = Result::Error;
          break;
      }
      break;

    case CommState::QueryStx:
      if (byte == STX) {
        comm_state = CommState::QueryEtx;
      } else if (byte == ACK) {
        ESP_LOGD(TAG, "Rx STX: Unuexpected extra ACK, ignoring"); // on rare occasions my unit will do this
      } else {
        ESP_LOGW(TAG, "Rx STX: Unuexpected 0x%02X", byte);
        comm_state = CommState::Cooldown;
        cooldown_length = S21_ERROR_TIMEOUT;
        result = Result::Error;
      }
      break;

    case CommState::QueryEtx:
      if (byte != ETX) {
        // not the end, add to buffer
        response.push_back(byte);
        if (response.size() > (S21_MAX_COMMAND_SIZE + S21_PAYLOAD_SIZE + 1)) {  // +1 for checksum byte
          ESP_LOGW(TAG, "Rx ETX: Overflow %s %s + 0x%02X",
            str_repr(response.data(), response.size()).c_str(),
            hex_repr(response.data(), response.size()).c_str(),
            byte);
          comm_state = CommState::Cooldown;
          cooldown_length = S21_ERROR_TIMEOUT;
          result = Result::Error;
        }
      } else {
        // frame received, validate checksum
        const uint8_t checksum = response[response.size() - 1];
        response.pop_back();
        const uint8_t calc_checksum = std::accumulate(response.begin(), response.end(), 0U);
        if ((calc_checksum == checksum)
            || ((calc_checksum == STX) && (checksum == ENQ))) {  // protocol avoids STX in message body
          tx_uart->write_byte(ACK);
          comm_state = CommState::Cooldown;
          cooldown_length = S21_RESPONSE_TURNAROUND;
          result = Result::Ack;
        } else {
          ESP_LOGW(TAG, "Rx ETX: Checksum mismatch: 0x%02X != 0x%02X (calc from %s)",
            checksum, calc_checksum, hex_repr(response.data(), response.size()).c_str());
          comm_state = CommState::Cooldown;
          cooldown_length = S21_ERROR_TIMEOUT;
          result = Result::Error;
        }
      }
      break;
      
    default:
      break;
  }

  return result;
}

DaikinSerial::Result DaikinSerial::service() {
  Result result = Result::Busy;

  switch(comm_state) {
  case CommState::Idle:
    result = Result::Idle;
    break;

  case CommState::Cooldown:
    if ((millis() - last_event_time) > cooldown_length) {
      comm_state = CommState::Idle;
      result = Result::Idle;
    }
    break;

  default:
    // all other states are actively receiving data from the unit
    if ((millis() - last_event_time) > S21_RESPONSE_TIMEOUT) {  // timed out?
      comm_state = CommState::Idle;
      result = Result::Timeout;
      break;
    }
    while ((result == Result::Busy) && rx_uart->available()) {  // read all available bytes
      uint8_t byte;
      rx_uart->read_byte(&byte);
      last_event_time = millis();
      result = handle_rx(byte);
    }
    break;
  }

  return result;
}

DaikinSerial::Result DaikinSerial::send_frame(const char *cmd, const std::array<char, S21_PAYLOAD_SIZE> *payload) {
  if (comm_state != CommState::Idle) {
    return Result::Busy;
  }

  size_t cmd_len = strlen(cmd);
  if (cmd_len > S21_MAX_COMMAND_SIZE) {
    ESP_LOGW(TAG, "Tx: Command '%s' too large", cmd);
    return Result::Error;
  }

  if (debug) {
    if (payload == nullptr) {
      ESP_LOGD(TAG, "Tx: %s", cmd);
    } else {
      ESP_LOGD(TAG, "Tx: %s %s %s", cmd,
               str_repr(reinterpret_cast<const uint8_t*>(payload->data()), payload->size()).c_str(),
               hex_repr(reinterpret_cast<const uint8_t*>(payload->data()), payload->size()).c_str());
    }
  }

  // prepare for response
  response.clear();
  flush_input();

  // transmit
  tx_uart->write_byte(STX);
  tx_uart->write_array(reinterpret_cast<const uint8_t *>(cmd), cmd_len);
  uint8_t checksum = std::accumulate(cmd, cmd + cmd_len, 0U);
  if (payload) {
    tx_uart->write_array(reinterpret_cast<const uint8_t *>(payload->data()), payload->size());
    checksum = std::accumulate(payload->data(), payload->data() + payload->size(), checksum);
  }
  if (checksum == STX) {
    checksum = ENQ;  // mid-message STX characters are escaped
  }
  tx_uart->write_byte(checksum);
  tx_uart->write_byte(ETX);

  // wait for result
  last_event_time = millis();
  comm_state = (payload != nullptr) ? CommState::CommandAck : CommState::QueryAck;

  return Result::Ack;
}

void DaikinSerial::flush_input() {  // would be a nice ESPHome API improvement
  uint8_t byte;
  while (rx_uart->available()) {
    rx_uart->read_byte(&byte);
  }
}

void DaikinS21::refine_queries() {
  // exclude F9 if unit supports the individual sensors with better resolution
  if (support_Ra && support_RH)
  {
    const auto f9 = std::find_if(std::begin(queries), std::end(queries), [](const char * query){ return strcmp(query, "F9") == 0; });
    if (f9 != std::end(queries))
    {
      ESP_LOGD(TAG, "Removing F9 from query pool (better support in Ra and RH)");
      queries.erase(f9);
    }
  }
}

void DaikinS21::tx_next() {
  std::array<char, DaikinSerial::S21_PAYLOAD_SIZE> payload;

  // select next command / query
  if (activate_climate) {
    tx_command = "D1";
    payload[0] = pending.power_on ? '1' : '0';
    payload[1] = static_cast<char>(pending.mode);
    payload[2] = c10_to_setpoint_byte(lroundf(round(pending.setpoint * 2) / 2 * 10.0));
    payload[3] = static_cast<char>(pending.fan);
    this->serial.send_frame(tx_command, &payload);
    return;
  }

  if (activate_swing_mode) {
    // todo encoding deviates from faikin
    tx_command = "D5";
    payload[0] = '0' + (pending.swing_h && pending.swing_v ? 4 : 0)
                     + (pending.swing_h ? 2 : 0)
                     + (pending.swing_v ? 1 : 0);
    payload[1] = pending.swing_v || pending.swing_h ? '?' : '0';
    payload[2] = '0';
    payload[3] = '0';
    this->serial.send_frame(tx_command, &payload);
    return;
  }
  
  // periodic queries
  if (current_query != queries.end()) {
    tx_command = *current_query;  // query scan underway, continue
    this->serial.send_frame(tx_command);
    return;
  }
  
  // start fresh query scan (only after current scan is complete)
  if (refresh_state) {
    refresh_state = false;
    refine_queries();
    current_query = queries.begin(); 
    tx_command = *current_query;
    this->serial.send_frame(tx_command);
    return;
  }
}

void DaikinS21::parse_ack() {
  char rcode[DaikinSerial::S21_MAX_COMMAND_SIZE + 1] = {};
  uint8_t payload[DaikinSerial::S21_PAYLOAD_SIZE];
  const size_t rcode_len = strlen(tx_command);
  size_t payload_len = 0;

  // prepare response buffers for decoding
  if (serial.response.empty()) {
    payload_len = 0;
    // commands don't return anything except an Ack, pretend we received the command itself to provide something to distinguish handling below
    std::copy_n(tx_command, rcode_len, rcode);
  } else {
    payload_len = serial.response.size() - rcode_len;
    std::copy_n(serial.response.data(), rcode_len, rcode);
    std::copy_n(serial.response.data() + rcode_len, payload_len, payload);
    // query successful, move to the next one
    current_query++;
  }

  switch (rcode[0]) {
    case 'G':  // F -> G
      switch (rcode[1]) {
        case '1':  // F1 -> Basic State
          this->active.power_on = (payload[0] == '1');
          this->active.mode = static_cast<DaikinClimateMode>(payload[1]);
          this->active.setpoint = ((payload[2] - 28) * 5);  // Celsius * 10
          if (this->support_RG == false) {  // prefer RG (silent mode not reported here)
            this->active.fan = static_cast<DaikinFanMode>(payload[3]);
          }
          this->ready.set(ReadyBasic);
          return;
        case '5':  // F5 -> G5 -- Swing state
          this->active.swing_v = payload[0] & 1;
          this->active.swing_h = payload[0] & 2;
          this->ready.set(ReadySwing);
          break;
        case '8':  // F8 -> G8 -- Protocol version. Always 0 for me.
          break;
        case '9':  // F9 -> G9 -- Temperature, better support in RH and Ra (0.5 degree granularity)
          this->temp_inside = temp_f9_byte_to_c10(&payload[0]);
          this->temp_outside = temp_f9_byte_to_c10(&payload[1]);
          break;
        default:
          break;
      }
      break;

    case 'S':  // R -> S
      switch (rcode[1]) {
        case 'B':  // Operational mode, single character, same info as G1
          return;
        case 'G':  // Fan mode, better detail than G1 (reports quiet mode)
          this->active.fan = static_cast<daikin_s21::DaikinFanMode>(payload[0]);
          this->support_RG = true;
          return;
        case 'H':  // Inside temperature
          this->temp_inside = temp_bytes_to_c10(payload);
          this->support_RH = true;
          return;
        case 'I':  // Coil temperature
          this->temp_coil = temp_bytes_to_c10(payload);
          return;
        case 'a':  // Outside temperature
          this->temp_outside = temp_bytes_to_c10(payload);
          this->support_Ra = true;
          return;
        case 'L':  // Fan speed
          this->fan_rpm = bytes_to_num(payload, payload_len) * 10;
          return;
        case 'd':  // Compressor frequency in hertz, idle if 0.
          this->compressor_hz = bytes_to_num(payload, payload_len);
          this->ready.set(ReadyCompressor);
          return;
        case 'C':  // Setpoint, same info as G1
          this->active.setpoint = bytes_to_num(payload, payload_len);
          return;
        case 'N':
          this->swing_vertical_angle = bytes_to_num(payload, 4);
          return;
        case 'F':  // Swing mode, same info as G5. ascii hex string: 2F herizontal 1F vertical 7F both 00 off
          break;
        case 'M':  // related to v_swing somehow
        case 'X':
        case 'z':
        default:
          if (payload_len > 3) {
            int8_t temp = temp_bytes_to_c10(payload);
            ESP_LOGD(TAG, "Unknown sensor: %s -> %s -> %.1f C (%.1f F)", rcode,
                     hex_repr(payload, payload_len).c_str(), c10_c(temp),
                     c10_f(temp));
          }
          break;
      }
      break;

    case 'M':  // todo faikin says 100WH units of power
      break;

    case 'D':  // D -> D (fake response, see above)
      switch (rcode[1]) {
        case '1':
          this->activate_climate = false;
          break;
        case '5':
          this->activate_swing_mode = false;
          break;
        default:
          break;
      }
      this->refresh_state = true; // a command took, trigger immediate refresh
      return;

    default:
      break;
  }

  // protocol decoding debug
  // note: well known responses return directly from the switch statements
  // break instead if you want to view their contents

  // print everything
  // if (this->debug_protocol) {
  //   ESP_LOGD(TAG, "S21: %s -> %s %s", rcode,
  //            str_repr(payload, payload_len).c_str(),
  //            hex_repr(payload, payload_len).c_str());
  // }

  // print changed values
  if (this->debug_protocol) {
    auto curr = std::vector<uint8_t>(payload, payload + payload_len);
    if (val_cache[rcode] != curr) {
      const auto prev = val_cache[rcode];
      ESP_LOGI(TAG, "S21 %s changed: %s %s -> %s %s", rcode,
               str_repr(prev.data(), prev.size()).c_str(),
               hex_repr(prev.data(), prev.size()).c_str(),
               str_repr(curr.data(), curr.size()).c_str(),
               hex_repr(curr.data(), curr.size()).c_str());
      val_cache[rcode] = curr;
    }
  }
}

void DaikinS21::handle_nak() {
  ESP_LOGW(TAG, "Rx: NAK from S21 for %s", tx_command);
  if (strcmp(tx_command, *current_query) == 0) {
    ESP_LOGW(TAG, "Removing %s from query pool (assuming unsupported)", tx_command);
    // current_query iterator will be invalidated, recover index and 
    const auto index = std::distance(queries.begin(), current_query);
    queries.erase(current_query);
    current_query = queries.begin() + index;
  } else {
    ESP_LOGW(TAG, "Acknowledging %s command despite NAK", tx_command);
    parse_ack();  // don't get stuck retrying unsupported command
  }
}

void DaikinS21::setup() {
  // populate messages to poll
  // clang-format off
#define S21_EXPERIMENTS 1
  queries = {
      "F1", "F5", "F9",
      "Rd", "RH", "RI", "Ra", "RL", "RG",
      // redundant/worse: "RC", "RF", "RB",
#if S21_EXPERIMENTS
      // Observed BRP device querying these.
      // "F2", "F3", "F4", "RN",
      // "RX", "RD", "M", "FU0F",
      // Query Experiments
      // "RA", 
      // "RE",
      // "RK", "RM", "RW",
      // "Rb", "Re", "Rg", "Rz",
#endif
  };
  // clang-format on

  current_query = queries.begin();
}

void DaikinS21::loop() {
  using Result = DaikinSerial::Result;

  switch (serial.service()) {
    case Result::Ack:
      ESP_LOGV(TAG, "Rx: ACK from S21 for command %s", tx_command);
      parse_ack();
      break;

    case Result::Idle:
      tx_next();
      break;

    case Result::Nak:
      handle_nak();
      break;

    case Result::Error:
      current_query = queries.end();
      refresh_state = true;
      activate_climate = false;
      activate_swing_mode = false;
      break;

    case Result::Timeout:
      ESP_LOGW(TAG, "Timeout waiting for response to %s", tx_command);
      break;

    default:
    break;
  }
}

void DaikinS21::update() {
  refresh_state = true;

  static bool ready_printed = false;
  if (!ready_printed && this->is_ready()) {
    ESP_LOGI(TAG, "Daikin S21 Ready");
    ready_printed = true;
  }

  if (this->debug_protocol) {
    this->dump_state();
  }
}

void DaikinS21::dump_state() {
  ESP_LOGD(TAG, "** BEGIN STATE *****************************");

  ESP_LOGD(TAG, "  Power: %s", ONOFF(this->active.power_on));
  ESP_LOGD(TAG, "   Mode: %s (%s)",
           daikin_climate_mode_to_string(this->active.mode).c_str(),
           this->is_idle() ? "idle" : "active");
  float degc = this->active.setpoint / 10.0;
  float degf = degc * 1.8 + 32.0;
  ESP_LOGD(TAG, " Target: %.1f C (%.1f F)", degc, degf);
  ESP_LOGD(TAG, "    Fan: %s (%d rpm)",
           daikin_fan_mode_to_string(this->active.fan).c_str(), this->fan_rpm);
  ESP_LOGD(TAG, "  Swing: H:%s V:%s", YESNO(this->active.swing_h),
           YESNO(this->active.swing_h));
  ESP_LOGD(TAG, " Inside: %.1f C (%.1f F)", c10_c(this->temp_inside),
           c10_f(this->temp_inside));
  ESP_LOGD(TAG, "Outside: %.1f C (%.1f F)", c10_c(this->temp_outside),
           c10_f(this->temp_outside));
  ESP_LOGD(TAG, "   Coil: %.1f C (%.1f F)", c10_c(this->temp_coil),
           c10_f(this->temp_coil));

  ESP_LOGD(TAG, "** END STATE *****************************");
}

void DaikinS21::set_daikin_climate_settings(bool power_on,
                                            DaikinClimateMode mode,
                                            float setpoint,
                                            DaikinFanMode fan_mode) {
  pending.power_on = power_on;
  pending.mode = mode;
  pending.setpoint = setpoint;
  pending.fan = fan_mode;
  activate_climate = true;
}

void DaikinS21::set_swing_settings(const bool swing_v, const bool swing_h) {
  pending.swing_v = swing_v;
  pending.swing_h = swing_h;
  activate_swing_mode = true;
}

}  // namespace daikin_s21
}  // namespace esphome
