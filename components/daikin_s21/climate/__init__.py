"""
Daikin S21 Mini-Split ESPHome component config validation & code generation.
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, sensor
from esphome.components.climate import ClimateMode
from esphome.const import (
    CONF_SUPPORTED_MODES,
)
from .. import (
    daikin_s21_ns,
    CONF_S21_ID,
    S21_CLIENT_SCHEMA,
    DaikinS21Client,
)

CONF_ROOM_TEMPERATURE_SENSOR = "room_temperature_sensor"
CONF_SUPPORTS_HUMIDITY = "supports_humidity"
CONF_SETPOINT_INTERVAL = "setpoint_interval"

DaikinS21Climate = daikin_s21_ns.class_(
    "DaikinS21Climate", climate.Climate, cg.Component, DaikinS21Client
)

SUPPORTED_CLIMATE_MODES_OPTIONS = {
    "OFF": ClimateMode.CLIMATE_MODE_OFF,  # always available
    "HEAT_COOL": ClimateMode.CLIMATE_MODE_HEAT_COOL,  # always available
    "COOL": ClimateMode.CLIMATE_MODE_COOL,
    "HEAT": ClimateMode.CLIMATE_MODE_HEAT,
    "FAN_ONLY": ClimateMode.CLIMATE_MODE_FAN_ONLY,
    "DRY": ClimateMode.CLIMATE_MODE_DRY,
}

CONFIG_SCHEMA = cv.All(
    climate.climate_schema(DaikinS21Climate)
    .extend(
        {
            cv.Optional(CONF_ROOM_TEMPERATURE_SENSOR): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_SETPOINT_INTERVAL, default="300s"): cv.positive_time_period_seconds,
            cv.Optional(CONF_SUPPORTED_MODES): cv.ensure_list(
                cv.enum(SUPPORTED_CLIMATE_MODES_OPTIONS, upper=True)
            ),
            cv.Optional(CONF_SUPPORTS_HUMIDITY): cv.boolean,
        }
    )
    .extend(S21_CLIENT_SCHEMA)
)

async def to_code(config):
    """Generate code"""
    var = await climate.new_climate(config)
    await cg.register_component(var, config)
    s21_var = await cg.get_variable(config[CONF_S21_ID])
    cg.add(var.set_s21(s21_var))

    if CONF_ROOM_TEMPERATURE_SENSOR in config:
        sens = await cg.get_variable(config[CONF_ROOM_TEMPERATURE_SENSOR])
        cg.add(var.set_room_sensor(sens))
        if CONF_SETPOINT_INTERVAL in config:
            cg.add(var.set_setpoint_interval(config[CONF_SETPOINT_INTERVAL]))

    if CONF_SUPPORTED_MODES in config:
        cg.add(var.set_supported_modes_override(config[CONF_SUPPORTED_MODES]))
    
    if CONF_SUPPORTS_HUMIDITY in config:
        cg.add(var.set_supports_current_humidity(config[CONF_SUPPORTS_HUMIDITY]))
