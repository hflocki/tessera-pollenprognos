# Pollen forecast

The pollen levels of the allergens you choose, from the DWD Pollenflug integration in Home Assistant: a row per
allergen with its name, and for today and the coming days a circle whose ring fills from none to high in green,
amber or red, the allergen's icon in its middle and the level in words under it. The strongest allergens first. It is the idea of the
Lovelace [pollenprognos-card](https://github.com/krissen/pollenprognos-card), drawn on a Tessera screen.

The circles grow with the tile: three allergens fill a tile two cells high, a taller one shows more. With no
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

- **Look**: circles with the level in words under them (the default), circles only, or bars that fill from none to
  high, which take less room on a small tile.
- **Days**: today alone, today and tomorrow, or three days.
- **Show allergens without pollen**: off leaves out an allergen with no pollen on every day shown.
- **Strongest first**: off keeps the order of the season (alder, hazel, ash, birch, grasses, rye, mugwort, ragweed).
- **Alder** to **Ragweed**: which allergens this tile shows, so two tiles can show different ones.

## Card and top bar

- A tap on the tile opens a card with every allergen that has a sensor, today and the two days after.
- The top bar item **Strong pollen** (Top bar, Add, From plugins) says "Birch: high" while the strongest allergen of
  today reaches a level. On the screen (hold the top bar, Settings, Plugins, Pollen forecast) or in the plugin's
  details: **In the top bar** on or off, and **From level** 1 (very low) to 6 (high), 5 at first.

## Good to know

- The screen reads the chosen sensors itself, today's level from their state and the next days from their attributes
  `state_tomorrow` and `state_in_2_days`. A change of a sensor in the plugin's details builds the screen again; the
  tile's options do not.
- The DWD publishes its forecast once a day, around 11 o'clock.

## A hobby project

This plugin is a private, non-commercial hobby project. I build and maintain it in my spare time and share it
free of charge as open source under the [MIT License](LICENSE). It is free and will stay free: there is no paid
version, no paid feature and no paid support.

Bug reports and ideas are welcome, but please keep in mind:

- There is no support and no guaranteed response time. I answer when I find the time, which can take days or weeks.
- Not every request gets built. I add what fits the plugin and what I can keep working over time.
- Pull requests are welcome; for a larger change, open an issue first so we can agree on the way.

### No warranty, no liability

The plugin is provided "as is", without any warranty and without guaranteed support, updates or fixes. You use it at
your own risk. To the extent permitted by law, I accept no liability for any damage or data loss resulting from its
use. The full terms are in the [MIT License](LICENSE).

### Buy me a coffee

If the plugin is useful to you and you would like to say thanks, you are welcome to buy me a coffee. A donation is
entirely voluntary: it is a thank-you, not a payment. It does not buy support, features, a response time or any other
service, and the plugin stays exactly the same for everyone, whether they donate or not.

[![Buy me a coffee](https://img.buymeacoffee.com/button-api/?text=Buy%20me%20a%20coffee&emoji=&slug=hflocki&button_colour=FFDD00&font_colour=000000&font_family=Cookie&outline_colour=000000&coffee_colour=ffffff)](https://www.buymeacoffee.com/hflocki)

☕ [buymeacoffee.com/hflocki](https://www.buymeacoffee.com/hflocki)
