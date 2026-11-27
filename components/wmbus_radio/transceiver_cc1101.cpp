#include "transceiver_cc1101.h"

#include "esphome/core/log.h"

#include <cstring>

namespace esphome {
namespace wmbus_radio {
static const char *TAG = "CC1101";

// CC1101 crystal frequency (standard 26 MHz)
#define F_XTAL 26000000

void CC1101::setup() {
  this->common_setup();
  this->configure_radio_();
}

// Réinitialise le CC1101 (reset logiciel puis tous les registres) et le remet en réception.
// Appelée au démarrage et en secours quand la puce ne répond plus (MARCSTATE à 0x00 = SLEEP
// alors que le code n'a jamais demandé la mise en veille : chute d'alimentation ou bus SPI).
void CC1101::configure_radio_() {
  ESP_LOGV(TAG, "Setup");

  // CC1101 has no hardware reset pin — skip hardware reset, use software reset only
  ESP_LOGVV(TAG, "software reset (SRES)");
  this->strobe(CC1101_SRES);
  delay(10);

  ESP_LOGVV(TAG, "checking part number");
  uint8_t partnum = this->read_status_register(CC1101_PARTNUM);
  uint8_t version = this->read_status_register(CC1101_VERSION);
  ESP_LOGVV(TAG, "part: %02X, version: %02X", partnum, version);
  if (partnum != 0x00) {
    ESP_LOGE(TAG, "Invalid part number: %02X (expected 0x00)", partnum);
    return;
  }

  // Configure for wM-Bus Mode C/T at 868.95 MHz, 100 kbps, 2-FSK
  // RF settings based on TI Design Note DN022 and wmbusmeters reference

  ESP_LOGVV(TAG, "configuring GDO pins");
  // GDO2: Sync word sent/received (active low, inverted)
  this->write_register(CC1101_IOCFG2, CC1101_GDO_SYNC_WORD | 0x40);
  // GDO1: High impedance (unused, default)
  this->write_register(CC1101_IOCFG1, CC1101_GDO_HI_Z);
  // Correctif (dépôt esphome-compteur-eau-izar) : GDO0 signale le mot de
  // synchro (actif bas, reste actif jusqu'au redémarrage RX) au lieu du seuil
  // de FIFO. GDO0 ne bascule plus au 32e octet, en pleine trame, là où les
  // trames T1 étaient systématiquement corrompues. Même principe que la PR
  // CC1101 d'IoTLabs-pl/ESPHome-Components (#107).
  this->write_register(CC1101_IOCFG0, CC1101_GDO_SYNC_WORD | 0x40);

  ESP_LOGVV(TAG, "configuring FIFO threshold");
  // FIFO threshold: 7 = RX FIFO >= 32 bytes, TX FIFO <= 31 bytes
  this->write_register(CC1101_FIFOTHR, 0x07);

  ESP_LOGVV(TAG, "configuring sync word");
  // Sync word for wM-Bus: 0x543D (with Manchester encoding: preamble 0x54)
  this->write_register(CC1101_SYNC1, WMBUS_SYNC_WORD_HIGH);
  this->write_register(CC1101_SYNC0, WMBUS_SYNC_WORD_LOW);

  ESP_LOGVV(TAG, "configuring packet control");
  // Correctif (longueur fixe) : une trame T1 de format A d'un compteur Izar (L = 0x19) fait
  // 45 octets une fois codée en 3 parmi 6. Le CC1101 reçoit donc exactement 45 octets puis
  // repasse seul au repos : toute la trame reste dans la FIFO (64 octets), et le pilote ne
  // la lit qu'après sa fin, jamais pendant la réception.
  this->write_register(CC1101_PKTLEN, WMBUS_T1_IZAR_ENCODED_LEN);
  // Packet control 1: No address check, no append status, no CRC autoflush
  this->write_register(CC1101_PKTCTRL1, 0x00);
  // Packet control 0: longueur fixe (PKTLEN), pas de blanchiment, pas de CRC
  this->write_register(CC1101_PKTCTRL0, 0x00);

  // No device address filtering
  this->write_register(CC1101_ADDR, 0x00);
  // Channel number 0
  this->write_register(CC1101_CHANNR, 0x00);

  ESP_LOGVV(TAG, "configuring frequency synthesizer");
  // IF frequency: ~152 kHz (typical for 100 kbps)
  this->write_register(CC1101_FSCTRL1, 0x08);
  this->write_register(CC1101_FSCTRL0, 0x00);

  ESP_LOGVV(TAG, "setting radio frequency");
  // FREQ = (f_carrier * 2^16) / f_xtal
  uint32_t freq_reg = ((uint64_t)this->frequency_hz_ << 16) / F_XTAL;
  this->write_register(CC1101_FREQ2, BYTE(freq_reg, 2));
  this->write_register(CC1101_FREQ1, BYTE(freq_reg, 1));
  this->write_register(CC1101_FREQ0, BYTE(freq_reg, 0));

  ESP_LOGVV(TAG, "configuring modem");
  // Modem configuration for 100 kbps, 2-FSK, 50 kHz deviation
  // MDMCFG4: Channel bandwidth and data rate exponent
  // BW = f_xtal / (8 * (4 + CHANBW_M) * 2^CHANBW_E) = 26M / (8 * 4 * 4) = ~203 kHz
  // 0x5C / 0x04 : valeurs identiques a maciekn/izar-wmbus-esp, zibous, Albert221 et ryszardlet
  // (configurations Izar T1 sur CC1101 qui fonctionnent, base TI SWRA234A). Par le calcul de la
  // datasheet (p.76) cela donne 103,15 kBaud et 325 kHz de bande.
  this->write_register(CC1101_MDMCFG4, 0x5C);
  // MDMCFG3: Data rate mantissa
  this->write_register(CC1101_MDMCFG3, 0x04);
  // MDMCFG2: Modulation format (2-FSK), sync mode (16/16 sync word bits)
  this->write_register(CC1101_MDMCFG2, 0x06);
  // MDMCFG1: FEC disabled, preamble 4 bytes
  this->write_register(CC1101_MDMCFG1, 0x22);
  // MDMCFG0: Channel spacing exponent
  this->write_register(CC1101_MDMCFG0, 0xF8);

  ESP_LOGVV(TAG, "configuring deviation");
  // Deviation: ~50 kHz
  // DEVIATION = (f_xtal / 2^17) * (8 + DEVIATION_M) * 2^DEVIATION_E
  // Valeur d'origine (38 kHz), voir ci-dessus
  this->write_register(CC1101_DEVIATN, 0x44);

  ESP_LOGVV(TAG, "configuring state machine");
  // MCSM2: RX time qualifier enabled
  this->write_register(CC1101_MCSM2, 0x07);
  // MCSM1: CCA mode, RX after RX, IDLE after TX
  this->write_register(CC1101_MCSM1, 0x00);
  // MCSM0: Auto calibrate from IDLE to RX/TX, PO timeout
  this->write_register(CC1101_MCSM0, 0x18);

  ESP_LOGVV(TAG, "configuring AFC/AGC");
  // FOCCFG: Frequency offset compensation
  this->write_register(CC1101_FOCCFG, 0x2E);
  // BSCFG: Bit synchronization
  this->write_register(CC1101_BSCFG, 0xBF);
  // AGC control
  this->write_register(CC1101_AGCCTRL2, 0x43);
  this->write_register(CC1101_AGCCTRL1, 0x09);
  // AGCCTRL0 0xB5 : identique aux quatre configurations Izar de reference (gain fige apres synchro).
  this->write_register(CC1101_AGCCTRL0, 0xB5);

  ESP_LOGVV(TAG, "configuring WOR");
  // Wake-on-Radio: disabled (but configured for compatibility)
  this->write_register(CC1101_WOREVT1, 0x87);
  this->write_register(CC1101_WOREVT0, 0x6B);
  this->write_register(CC1101_WORCTRL, 0xFB);

  ESP_LOGVV(TAG, "configuring front end");
  // Front end RX: LNA and mixer current
  this->write_register(CC1101_FREND1, 0xB6);
  // Front end TX: PA power index
  this->write_register(CC1101_FREND0, 0x10);

  ESP_LOGVV(TAG, "configuring frequency calibration");
  // Frequency synthesizer calibration
  this->write_register(CC1101_FSCAL3, 0xEA);
  this->write_register(CC1101_FSCAL2, 0x2A);
  this->write_register(CC1101_FSCAL1, 0x00);
  this->write_register(CC1101_FSCAL0, 0x1F);

  // RC oscillator configuration
  this->write_register(CC1101_RCCTRL1, 0x41);
  this->write_register(CC1101_RCCTRL0, 0x00);

  ESP_LOGVV(TAG, "configuring test registers");
  // Test registers for optimal performance
  this->write_register(CC1101_FSTEST, 0x59);
  this->write_register(CC1101_PTEST, 0x7F);
  this->write_register(CC1101_AGCTEST, 0x3F);
  this->write_register(CC1101_TEST2, 0x81);
  this->write_register(CC1101_TEST1, 0x35);
  this->write_register(CC1101_TEST0, 0x09);

  ESP_LOGVV(TAG, "calibrating");
  // Calibrate frequency synthesizer
  this->strobe(CC1101_SCAL);
  delay(1);

  ESP_LOGVV(TAG, "entering RX mode");
  // Flush RX FIFO and enter RX mode
  this->strobe(CC1101_SFRX);
  this->strobe(CC1101_SRX);

  ESP_LOGV(TAG, "CC1101 setup done");
}

uint8_t CC1101::strobe(uint8_t cmd) {
  this->delegate_->begin_transaction();
  uint8_t status = this->delegate_->transfer(cmd);
  this->delegate_->end_transaction();
  return status;
}

uint8_t CC1101::read_register(uint8_t address) {
  this->delegate_->begin_transaction();
  this->delegate_->transfer(address | CC1101_READ_SINGLE);
  uint8_t value = this->delegate_->transfer(0x00);
  this->delegate_->end_transaction();
  return value;
}

uint8_t CC1101::read_status_register(uint8_t address) {
  this->delegate_->begin_transaction();
  this->delegate_->transfer(address | CC1101_READ_BURST);
  uint8_t value = this->delegate_->transfer(0x00);
  this->delegate_->end_transaction();
  return value;
}

void CC1101::write_register(uint8_t address, uint8_t value) {
  this->delegate_->begin_transaction();
  this->delegate_->transfer(address | CC1101_WRITE_SINGLE);
  this->delegate_->transfer(value);
  this->delegate_->end_transaction();
}

void CC1101::write_burst(uint8_t address, const uint8_t *data, size_t length) {
  this->delegate_->begin_transaction();
  this->delegate_->transfer(address | CC1101_WRITE_BURST);
  for (size_t i = 0; i < length; i++) {
    this->delegate_->transfer(data[i]);
  }
  this->delegate_->end_transaction();
}

void CC1101::read_burst(uint8_t address, uint8_t *data, size_t length) {
  this->delegate_->begin_transaction();
  this->delegate_->transfer(address | CC1101_READ_BURST);
  for (size_t i = 0; i < length; i++) {
    data[i] = this->delegate_->transfer(0x00);
  }
  this->delegate_->end_transaction();
}

uint8_t CC1101::get_rx_bytes() {
  return this->read_status_register(CC1101_RXBYTES) & 0x7F;
}

optional<uint8_t> CC1101::read() {
  // Check if IRQ pin indicates data available (active low = FIFO threshold reached)
  if (this->irq_pin_->digital_read() == false) {
    uint8_t rx_bytes = this->get_rx_bytes();
    if (rx_bytes > 0) {
      // Read RSSI before reading data byte (RSSI is valid after sync word)
      this->last_rssi_ = (int8_t)this->read_status_register(CC1101_RSSI);

      // Read single byte from RX FIFO
      uint8_t data;
      this->read_burst(CC1101_RXFIFO, &data, 1);
      return data;
    }
  }
  return {};
}

size_t CC1101::get_frame(uint8_t *buffer, size_t length, uint32_t offset) {
  // CC1101 reads byte-by-byte from FIFO (offset is ignored for FIFO-based reading)
  // Returns 1 on success, 0 if FIFO is empty (waiting for more data)
  auto byte = this->read();
  if (!byte.has_value())
    return 0;

  *buffer = *byte;
  return 1;
}

void CC1101::restart_rx() {
  ESP_LOGVV(TAG, "Restarting RX");

  // Go to IDLE state. Poll MARCSTATE instead of delay(1), bounded at 5 ms so a
  // missing or stuck chip cannot hang the radio task.
  this->strobe(CC1101_SIDLE);
  const uint8_t state = this->wait_marcstate_(CC1101_MARCSTATE_IDLE, 5000);
  if (state != CC1101_MARCSTATE_IDLE && state != CC1101_MARCSTATE_RXFIFO_OVERFLOW) {
    // SFRX is only valid in IDLE or RXFIFO_OVERFLOW: try again on the next restart.
    const uint32_t now = millis();
    if (now - this->last_idle_timeout_log_ >= 60000) {
      this->last_idle_timeout_log_ = now;
      ESP_LOGW(TAG, "CC1101 did not enter IDLE (MARCSTATE 0x%02X)", state);
    }
    // Correctif : la puce ne répond plus (SLEEP 0x00, ou SPI muet) : sans réinitialisation elle
    // resterait sourde pour toujours. On refait le reset et la configuration, au plus une fois
    // toutes les 5 s.
    if ((state == 0x00 || state == 0x1F) && now - this->last_reinit_ms_ >= 5000) {
      this->last_reinit_ms_ = now;
      ESP_LOGW(TAG, "CC1101 ne repond plus (MARCSTATE 0x%02X) : reinitialisation", state);
      this->configure_radio_();
    }
    return;
  }

  // Flush RX FIFO (also leaves RXFIFO_OVERFLOW)
  this->strobe(CC1101_SFRX);

  ulTaskNotifyTake(pdTRUE, 0);

  // Enter RX mode (calibrates on IDLE -> RX with FS_AUTOCAL)
  this->strobe(CC1101_SRX);
  this->wait_marcstate_(CC1101_MARCSTATE_RX, 5000);
}

// Polls MARCSTATE until it equals `state` or RXFIFO_OVERFLOW, or the timeout passes.
// Returns the last state read.
uint8_t CC1101::wait_marcstate_(uint8_t state, uint32_t timeout_us) {
  const uint32_t start = micros();
  uint8_t current;
  do {
    current = this->read_status_register(CC1101_MARCSTATE) & 0x1F;
  } while (current != state && current != CC1101_MARCSTATE_RXFIFO_OVERFLOW &&
           micros() - start < timeout_us);
  return current;
}

bool CC1101::idle_restart_ready() {
  // TI SWRS061I, PKTSTATUS: SFD bit 3, GDO2 bit 2, GDO0 bit 0.
  // GDO status is non-inverted, even with IOCFG2=0x46 and IOCFG0=0x40.
  // A set bit means sync/FIFO activity; do not refresh in that case.
  constexpr uint8_t active_mask = (1u << 3) | (1u << 2) | (1u << 0);
  if (this->read_status_register(CC1101_PKTSTATUS) & active_mask)
    return false;
  // Include the overflow bit: 0x80 must not look like an empty FIFO.
  return this->read_status_register(CC1101_RXBYTES) == 0;
}

int8_t CC1101::get_rssi() {
  // Convert RSSI_dec to dBm: RSSI_dBm = (RSSI_dec / 2) - RSSI_offset
  // RSSI offset per TI DN505: 74 dBm for 868 MHz, 76 dBm for 433 MHz
  int8_t rssi_offset = (this->frequency_hz_ < 600000000u) ? 76 : 74;
  uint8_t rssi_dec = this->last_rssi_;
  int16_t rssi_dbm;
  if (rssi_dec >= 128) {
    rssi_dbm = ((int16_t)(rssi_dec - 256) / 2) - rssi_offset;
  } else {
    rssi_dbm = (rssi_dec / 2) - rssi_offset;
  }
  return (int8_t)rssi_dbm;
}

const char *CC1101::get_name() { return TAG; }

bool CC1101::read_in_task(uint8_t *buffer, size_t length, uint32_t offset) {
  // Correctif (longueur fixe) : à l'appel avec offset 0 (début de trame), on attend que le
  // CC1101 ait fini de recevoir ses 45 octets (il repasse seul à l'état IDLE), on lit toute la
  // FIFO en une fois au repos, puis on rend les octets demandés aux appels suivants.
  if (offset == 0) {
    this->frame_len_ = 0;
    this->frame_pos_ = 0;
    // AUCUN accès SPI tant que la trame est en cours. Sur ce montage, chaque accès au bus
    // pendant la réception rend les bits suivants faux : les trames n'étaient justes que
    // jusqu'au premier accès (32e octet avec la lecture au seuil de la FIFO d'origine, tout
    // de suite avec la lecture continue du composant version_3, 3 premiers octets quand le
    // RSSI était lu dès le réveil). On attend donc la durée de la trame sans toucher au bus :
    // 45 octets codés à 100 kbit/s = 3,6 ms, plus une marge.
    delayMicroseconds(WMBUS_T1_IZAR_AIR_TIME_US);

    const uint32_t start = micros();
    uint8_t marcstate;
    while (true) {
      marcstate = this->read_status_register(CC1101_MARCSTATE) & 0x1F;
      if (marcstate == CC1101_MARCSTATE_IDLE || marcstate == CC1101_MARCSTATE_RXFIFO_OVERFLOW)
        break;
      // 45 octets à 100 kbit/s = 3,6 ms : 20 ms est largement suffisant.
      if (micros() - start > 20000) {
        ESP_LOGW(TAG, "Trame non terminee apres 20 ms (MARCSTATE 0x%02X)", marcstate);
        return false;
      }
      delayMicroseconds(100);
    }

    // Le RSSI est lu après la trame (le CC1101 garde la dernière valeur mesurée).
    this->last_rssi_ = (int8_t)this->read_status_register(CC1101_RSSI);

    const uint8_t rxbytes_raw = this->read_status_register(CC1101_RXBYTES);
    if ((rxbytes_raw & 0x80) || marcstate == CC1101_MARCSTATE_RXFIFO_OVERFLOW) {
      ESP_LOGW(TAG, "RX FIFO overflow");
      return false;
    }
    uint8_t available = rxbytes_raw & 0x7F;
    if (available == 0) {
      ESP_LOGV(TAG, "FIFO vide en fin de trame");
      return false;
    }
    if (available > sizeof(this->frame_buf_))
      available = sizeof(this->frame_buf_);
    this->read_burst(CC1101_RXFIFO, this->frame_buf_, available);
    this->frame_len_ = available;
    ESP_LOGD(TAG, "Trame lue : %u octets, RSSI brut %d", (unsigned) available, (int) this->last_rssi_);
  }

  if (this->frame_pos_ + length > this->frame_len_) {
    ESP_LOGD(TAG, "Trame trop courte : %u octets lus, %u demandes a partir de %u",
             (unsigned) this->frame_len_, (unsigned) length, (unsigned) this->frame_pos_);
    return false;
  }
  memcpy(buffer, this->frame_buf_ + this->frame_pos_, length);
  this->frame_pos_ += length;
  return true;
}

} // namespace wmbus_radio
} // namespace esphome
