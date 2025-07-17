"""
Sensor component for daikin_s21.
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_HUMIDITY,
    CONF_ID,
    UNIT_CELSIUS,
    UNIT_HERTZ,
    UNIT_PERCENT,
    UNIT_REVOLUTIONS_PER_MINUTE,
    ICON_FAN,
    ICON_THERMOMETER,
    ICON_WATER_PERCENT,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_WIND_DIRECTION,
    STATE_CLASS_MEASUREMENT,
)

from .. import (
    daikin_s21_ns,
    CONF_S21_ID,
    S21_CLIENT_SCHEMA,
    DaikinS21Client,
)

DaikinS21Sensor = daikin_s21_ns.class_(
    "DaikinS21Sensor", cg.PollingComponent, DaikinS21Client
)

CONF_S21_ID = "s21_id"
CONF_INSIDE_TEMP = "inside_temperature"
CONF_OUTSIDE_TEMP = "outside_temperature"
CONF_COIL_TEMP = "coil_temperature"
CONF_FAN_SPEED = "fan_speed"
CONF_SWING_VERTICAL_ANGLE = "swing_vertical_angle"
CONF_COMPRESSOR_FREQUENCY = "compressor_frequency"
CONF_DEMAND = "demand"

CONFIG_SCHEMA = (
    cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(DaikinS21Sensor),
            cv.Optional(CONF_INSIDE_TEMP): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_THERMOMETER,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_OUTSIDE_TEMP): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_THERMOMETER,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_COIL_TEMP): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_THERMOMETER,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_FAN_SPEED): sensor.sensor_schema(
                unit_of_measurement=UNIT_REVOLUTIONS_PER_MINUTE,
                icon=ICON_FAN,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_FREQUENCY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_SWING_VERTICAL_ANGLE): sensor.sensor_schema(
                icon="mdi:pan-vertical",
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_WIND_DIRECTION,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_COMPRESSOR_FREQUENCY): sensor.sensor_schema(
                unit_of_measurement=UNIT_HERTZ,
                icon="mdi:pump",
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_FREQUENCY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                icon=ICON_WATER_PERCENT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_DEMAND): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                icon="mdi:thermometer-chevron-up",
                accuracy_decimals=1,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(S21_CLIENT_SCHEMA)
    .extend(cv.polling_component_schema("10s"))
)


async def to_code(config):
    """Generate main.cpp code"""

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    s21_var = await cg.get_variable(config[CONF_S21_ID])
    cg.add(var.set_s21(s21_var))

    if CONF_INSIDE_TEMP in config:
        sens = await sensor.new_sensor(config[CONF_INSIDE_TEMP])
        cg.add(var.set_temp_inside_sensor(sens))

    if CONF_OUTSIDE_TEMP in config:
        sens = await sensor.new_sensor(config[CONF_OUTSIDE_TEMP])
        cg.add(var.set_temp_outside_sensor(sens))

    if CONF_COIL_TEMP in config:
        sens = await sensor.new_sensor(config[CONF_COIL_TEMP])
        cg.add(var.set_temp_coil_sensor(sens))

    if CONF_FAN_SPEED in config:
        sens = await sensor.new_sensor(config[CONF_FAN_SPEED])
        cg.add(var.set_fan_speed_sensor(sens))

    if CONF_SWING_VERTICAL_ANGLE in config:
        sens = await sensor.new_sensor(config[CONF_SWING_VERTICAL_ANGLE])
        cg.add(var.set_swing_vertical_angle_sensor(sens))

    if CONF_COMPRESSOR_FREQUENCY in config:
        sens = await sensor.new_sensor(config[CONF_COMPRESSOR_FREQUENCY])
        cg.add(var.set_compressor_frequency_sensor(sens))
    
    if CONF_HUMIDITY in config:
        sens = await sensor.new_sensor(config[CONF_HUMIDITY])
        cg.add(var.set_humidity_sensor(sens))

    if CONF_DEMAND in config:
        sens = await sensor.new_sensor(config[CONF_DEMAND])
        cg.add(var.set_demand_sensor(sens))
