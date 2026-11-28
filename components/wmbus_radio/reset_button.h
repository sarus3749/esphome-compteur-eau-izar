#pragma once

#include "esphome/components/button/button.h"
#include "esphome/core/component.h"

#include "component.h"

namespace esphome {
namespace wmbus_radio {

// Bouton Home Assistant : réinitialise le CC1101 (reset logiciel + tous les registres).
class ResetButton : public button::Button, public Component {
public:
  void set_radio(Radio *radio) { this->radio_ = radio; }

protected:
  void press_action() override {
    if (this->radio_ != nullptr)
      this->radio_->request_radio_reset();
  }

  Radio *radio_{nullptr};
};

} // namespace wmbus_radio
} // namespace esphome
