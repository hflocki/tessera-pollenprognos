"""Pollen forecast: the levels of the DWD Pollenflug sensors chosen for the plugin, today and the coming days.

plugin.yaml reads each allergen's sensor from Home Assistant (today, tomorrow, the day after); this component hands
those sensors to the tile (pollen.cpp). register_plugin() takes the id, version, tile memory and the screen's texts
from the manifest and translations/ beside this folder.
"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number, sensor, smart_display, switch
from esphome.const import CONF_ID

DEPENDENCIES = ["smart_display"]
AUTO_LOAD = ["sensor", "switch", "number"]

CONF_IN_BAR = "in_bar"
CONF_ALERT_LEVEL = "alert_level"

# The allergens in the order of the season; the index is the one the C++ uses.
ALLERGENS = ['erle', 'hasel', 'esche', 'birke', 'graeser', 'roggen', 'beifuss', 'ambrosia']

pollen_ns = cg.esphome_ns.namespace("pollen")
Pollen = pollen_ns.class_("Pollen", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Pollen),
        cv.Optional(CONF_IN_BAR): cv.use_id(switch.Switch),
        cv.Optional(CONF_ALERT_LEVEL): cv.use_id(number.Number),
    }
    | {
        cv.Optional(name): cv.All(cv.ensure_list(cv.use_id(sensor.Sensor)), cv.Length(min=3, max=3))
        for name in ALLERGENS
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await smart_display.register_plugin(var, __file__)
    if CONF_IN_BAR in config:
        cg.add(var.set_in_bar(await cg.get_variable(config[CONF_IN_BAR])))
    if CONF_ALERT_LEVEL in config:
        cg.add(var.set_alert_level(await cg.get_variable(config[CONF_ALERT_LEVEL])))
    for index, name in enumerate(ALLERGENS):
        if name not in config:
            continue
        for day, sensor_id in enumerate(config[name]):
            cg.add(var.set_sensor(index, day, await cg.get_variable(sensor_id)))
