# Pollen forecast

The pollen levels of the allergens you choose, from the DWD Pollenflug integration in Home Assistant: a row per
allergen with its icon in the colour of today's level and its name, and for today and the coming days a bar that fills
from none to high in green, amber or red, with the level in words. The strongest allergens first. It is the idea of the
Lovelace [pollenprognos-card](https://github.com/krissen/pollenprognos-card), drawn on a Tessera screen.

On a narrow tile the bars sit under each name, on a wide one beside it; a taller tile shows more allergens. With no
pollen in the air the tile shows a green flower and "No pollen".

## Set up

1. Have the [DWD Pollenflug integration](https://github.com/mampfes/hacs_dwd_pollenflug) in Home Assistant, set to
   your region. It makes a sensor per allergen, such as `sensor.pollenflug_birke_50`.
2. In Tessera, open **Plugins**, **Add with a link**, and paste the link of this repository (or copy the folder to
   `/config/tessera-plugins/pollen/`). Add the plugin to a screen.
3. In the plugin's details on the screen's Plugins tab, under **When building**, choose the sensor of each allergen you
   want (Alder, Hazel, Ash, Birch, Grasses, Rye, Mugwort, Ragweed); leave the others empty. **Save and build**.
4. In **Layout**, place the **Pollen forecast** tile and set in its inspector what it shows.

## Options of a tile

- **Days**: today alone, today and tomorrow, or three days.
- **Show allergens without pollen**: off leaves out an allergen with no pollen on every day shown.
- **Strongest first**: off keeps the order of the season (alder, hazel, ash, birch, grasses, rye, mugwort, ragweed).
- **Alder** to **Ragweed**: which allergens this tile shows, so two tiles can show different ones.

## Good to know

- The screen reads the chosen sensors itself, today's level from their state and the next days from their attributes
  `state_tomorrow` and `state_in_2_days`. A change of a sensor in the plugin's details builds the screen again; the
  tile's options do not.
- The DWD publishes its forecast once a day, around 11 o'clock.
