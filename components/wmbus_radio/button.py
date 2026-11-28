import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC

from . import RadioComponent, radio_ns

CONF_WMBUS_RADIO_ID = "wmbus_radio_id"

ResetButton = radio_ns.class_("ResetButton", button.Button, cg.Component)

CONFIG_SCHEMA = (
    button.button_schema(
        ResetButton,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        icon="mdi:radio-tower",
    )
    .extend({cv.GenerateID(CONF_WMBUS_RADIO_ID): cv.use_id(RadioComponent)})
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await button.new_button(config)
    await cg.register_component(var, config)
    radio = await cg.get_variable(config[CONF_WMBUS_RADIO_ID])
    cg.add(var.set_radio(radio))
