#include "pollen.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace esphome::pollen {

using tessera::Font;
using tessera::Tone;
namespace ui = tessera::ui;

// Shown or hidden, only when it differs: a hidden part is not drawn, and a part of a row not shown never lies outside.
static void show(lv_obj_t *obj, bool on) {
  if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN) == on) {
    if (on) lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
  }
}

// ---- Readable colours ---------------------------------------------------------------------------------------------

// A colour's luminance and the WCAG contrast of two: amber or yellow words on the light look's white card fall under
// what a person reads, so there they take the ink instead; on the dark look they keep their colour.
static float luminance(lv_color_t c) {
  auto channel = [](uint8_t v) {
    const float s = v / 255.0f;
    return s <= 0.03928f ? s / 12.92f : std::pow((s + 0.055f) / 1.055f, 2.4f);
  };
  return 0.2126f * channel(c.red) + 0.7152f * channel(c.green) + 0.0722f * channel(c.blue);
}
static bool reads_on_card(lv_color_t c) {
  const float a = luminance(c), b = luminance(ui::color(theme::CARD));
  return (std::max(a, b) + 0.05f) / (std::min(a, b) + 0.05f) >= 3.0f;
}
// A label (words or an icon) in a tone's or a role's colour where that reads on the card, else in the ink.
static void set_readable(lv_obj_t *label, tessera::Tone tone, theme::Role role) {
  if (reads_on_card(ui::tone_color(tone, role))) ui::set_tone(label, tone, role);
  else ui::set_color(label, theme::INK);
}

// The allergens' keys: option show_<key>, screen texts <key> (the name) and the order of the sensors.
static const char *const KEYS[ALLERGENS] = {"erle", "hasel", "esche", "birke", "graeser", "roggen", "beifuss", "ambrosia"};
// Their icons: trees, grasses and herbs.
static const uint32_t ICONS[ALLERGENS] = {0xF0531, 0xF0531, 0xF0531, 0xF0531, 0xF1510, 0xF1510, 0xF0E66, 0xF0E66};
static constexpr uint32_t ICON_FLOWER = 0xF024A;
static const char *const DAY_KEYS[DAYS] = {"today", "tomorrow", "in_2_days"};
static constexpr int MAX_ROWS = ALLERGENS;
static constexpr int STEPS = 6;  // DWD's 0.5 to 3 in steps of 0.5

// DWD's level 0 to 3 in steps of 0.5 as a step from 0 (none) to 6 (high); -1 for none known.
static int step_of(float level) {
  if (std::isnan(level) || level < 0) return -1;
  return std::min(STEPS, static_cast<int>(level * 2 + 0.25f));
}

// The colour of a step: nothing, then green for low, amber for moderate, red for high.
static Tone tone_of(int step) {
  if (step <= 0) return Tone::NORMAL;
  if (step <= 2) return Tone::GOOD;
  if (step <= 4) return Tone::BUSY;
  return Tone::ALERT;
}

static void place(lv_obj_t *obj, int x, int y, int w, int h) {
  lv_obj_set_size(obj, std::max(1, w), std::max(1, h));
  lv_obj_set_pos(obj, x, y);
}


// ---- Bars: a bar per day that fills from none to high ----


// A block per allergen: its icon in the colour of today's level and its name, and for each day a bar that fills from
// none to high in green, amber or red, with the level in words. On a narrow tile the bars sit under the name, on a wide
// one beside it. The blocks spread over the tile's height, the strongest first; with nothing to show the tile says so
// with a large flower.
class BarsTile : public tessera::Tile {
 public:
  explicit BarsTile(const Pollen *plugin) : plugin_(plugin) {}

  void create(const tessera::TileContext &c) override {
    width_ = c.width;
    height_ = c.height;
    days_ = std::max(1, std::min(DAYS, atoi(c.options["days"] | "2")));
    show_empty_ = c.options["show_empty"] | false;
    sort_ = c.options["sort"] | true;
    for (int a = 0; a < ALLERGENS; ++a) {
      char key[24];
      snprintf(key, sizeof key, "show_%s", KEYS[a]);
      chosen_[a] = c.options[key] | true;
    }

    title_h_ = ui::line_height(Font::TITLE);
    body_h_ = ui::line_height(Font::BODY);
    icon_h_ = ui::line_height(Font::ICON_SMALL);
    bar_h_ = std::max(ui::px(7), ui::mm(2) * 3 / 4);
    col_gap_ = ui::px(ui::large() ? 10 : 6);
    min_gap_ = ui::px(ui::large() ? 10 : 6);
    for (int s = 0; s <= STEPS; ++s) word_w_ = std::max(word_w_, ui::text_width(level_text(s), Font::BODY));
    int longest = 0;
    for (const char *key : KEYS) longest = std::max(longest, ui::text_width(plugin_->text(key), Font::TITLE));
    icon_w_ = icon_h_ + ui::px(6);

    // Beside the name when every day's bar keeps at least 18 mm next to a name column of the longest name.
    name_w_ = std::min(icon_w_ + longest + ui::px(10), width_ / 2);
    side_ = (width_ - name_w_ - (days_ - 1) * col_gap_) / days_ >= ui::mm(18);
    if (side_) {
      cell_x_ = name_w_;
      row_h_ = std::max(title_h_, body_h_ + ui::px(3) + bar_h_);
    } else {
      cell_x_ = 0;
      row_h_ = title_h_ + ui::px(4) + bar_h_;
    }
    cell_w_ = std::max(1, (width_ - cell_x_ - (days_ - 1) * col_gap_) / days_);
    header_ = days_ > 1;
    const int top = header_ ? body_h_ + ui::px(4) : 0;
    rows_ = std::max(1, std::min(MAX_ROWS, (height_ - top + min_gap_) / (row_h_ + min_gap_)));

    if (header_)
      for (int d = 0; d < days_; ++d) {
        head_[d] = ui::label(c.parent, Font::BODY, theme::MUTED);
        if (side_) lv_obj_set_style_text_align(head_[d], LV_TEXT_ALIGN_LEFT, 0);
        ui::set_text(head_[d], plugin_->text(DAY_KEYS[d]));
      }
    for (int r = 0; r < rows_; ++r) {
      Row &row = row_[r];
      row.icon = ui::label(c.parent, Font::ICON_SMALL, theme::MUTED);
      row.name = ui::label(c.parent, Font::TITLE, theme::INK);
      if (!side_) {
        row.today = ui::label(c.parent, Font::BODY, theme::MUTED);
        lv_obj_set_style_text_align(row.today, LV_TEXT_ALIGN_RIGHT, 0);
      }
      for (int d = 0; d < days_; ++d) {
        if (side_) row.word[d] = ui::label(c.parent, Font::BODY, theme::MUTED);
        row.track[d] = ui::block(c.parent, theme::TRACK);
        lv_obj_set_style_radius(row.track[d], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_pad_all(row.track[d], 0, 0);
        lv_obj_remove_flag(row.track[d], LV_OBJ_FLAG_SCROLLABLE);
        row.fill[d] = ui::block(row.track[d], theme::ACCENT);
        lv_obj_set_style_radius(row.fill[d], LV_RADIUS_CIRCLE, 0);
      }
    }
    // What the tile shows with nothing to show: a large flower and a word under it.
    big_ = ui::label(c.parent, Font::ICON, theme::MUTED);
    lv_obj_set_style_text_align(big_, LV_TEXT_ALIGN_CENTER, 0);
    note_ = ui::label(c.parent, Font::BODY_LARGE, theme::MUTED);
    lv_obj_set_style_text_align(note_, LV_TEXT_ALIGN_CENTER, 0);
    const int ih = ui::line_height(Font::ICON), nh = ui::line_height(Font::BODY_LARGE);
    const int y = std::max(0, (height_ - ih - nh) / 2);
    place(big_, 0, y, width_, ih);
    place(note_, 0, y + ih, width_, nh);
  }

  void on_tick(uint32_t) override {
    // The allergens to show: chosen in the inspector, with a sensor, and (unless asked) with pollen on a shown day.
    Item items[ALLERGENS];
    int count = 0, with_sensor = 0;
    for (int a = 0; a < ALLERGENS; ++a) {
      if (!chosen_[a]) continue;
      Item it{a, {-1, -1, -1}, -1};
      for (int d = 0; d < days_; ++d) {
        it.step[d] = step_of(plugin_->level(a, d));
        it.most = std::max(it.most, it.step[d]);
      }
      if (it.most < 0) continue;  // no sensor, or it has no value
      ++with_sensor;
      if (!show_empty_ && it.most <= 0) continue;
      items[count++] = it;
    }
    if (sort_)
      std::stable_sort(items, items + count, [](const Item &x, const Item &y) {
        return x.step[0] != y.step[0] ? x.step[0] > y.step[0] : x.most > y.most;
      });

    const char *why = with_sensor == 0 ? "choose" : count == 0 ? "none" : nullptr;
    ui::set_text(big_, why ? ui::icon(ICON_FLOWER) : "");
    ui::set_tone(big_, why && *why == 'n' ? Tone::GOOD : Tone::NORMAL, theme::MUTED);
    ui::set_text(note_, why ? plugin_->text(why) : "");

    // The blocks spread over the height under the header, as far apart as the room allows, a little more at most.
    const int shown = why ? 0 : std::min(count, rows_);
    const int top = header_ ? body_h_ + ui::px(4) : 0;
    const int free = height_ - top - shown * row_h_;
    const int gap = shown > 1 ? std::max(min_gap_, std::min(free / (shown - 1), row_h_)) : 0;
    const int used = shown * row_h_ + std::max(0, shown - 1) * gap;
    const int y0 = top + std::max(0, std::min(free, (height_ - top - used)) / 2);
    for (int d = 0; d < days_; ++d)
      if (head_[d]) {
        ui::set_text(head_[d], shown ? plugin_->text(DAY_KEYS[d]) : "");
        place(head_[d], cell_x_ + d * (cell_w_ + col_gap_), std::max(0, y0 - top), cell_w_, body_h_);
      }
    for (int r = 0; r < rows_; ++r) {
      if (r < shown) paint(row_[r], items[r], y0 + r * (row_h_ + gap));
      else clear(row_[r]);
    }
  }

  void on_theme() override {
    for (int d = 0; d < days_; ++d)
      if (head_[d]) ui::set_color(head_[d], theme::MUTED);
    ui::set_color(note_, theme::MUTED);
    // Everything else takes its colour on every tick.
  }

 private:
  struct Item {
    int allergen;
    int step[DAYS];
    int most;
  };
  struct Row {
    lv_obj_t *icon{}, *name{}, *today{};
    lv_obj_t *word[DAYS]{}, *track[DAYS]{}, *fill[DAYS]{};
  };

  std::string level_text(int step) const {
    char key[8];
    snprintf(key, sizeof key, "lvl%d", std::max(0, step));
    return plugin_->text(key);
  }

  void paint(Row &row, const Item &it, int y) {
    const Tone tone = tone_of(it.step[0]);
    ui::set_text(row.icon, ui::icon(ICONS[it.allergen]));
    set_readable(row.icon, tone, theme::MUTED);
    ui::set_text(row.name, plugin_->text(KEYS[it.allergen]));
    ui::set_color(row.name, theme::INK);
    if (side_) {
      const int line = (row_h_ - title_h_) / 2;
      place(row.icon, 0, y + (row_h_ - icon_h_) / 2, icon_h_, icon_h_);
      place(row.name, icon_w_, y + line, name_w_ - icon_w_ - ui::px(6), title_h_);
    } else {
      // The name on the first line, today's level in words at its right.
      const int ww = std::min(width_ / 2, word_w_ + ui::px(2));
      place(row.icon, 0, y + (title_h_ - icon_h_) / 2, icon_h_, icon_h_);
      place(row.name, icon_w_, y, width_ - icon_w_ - ww - ui::px(6), title_h_);
      place(row.today, width_ - ww, y + (title_h_ - body_h_) / 2, ww, body_h_);
      ui::set_text(row.today, level_text(it.step[0]));
      set_readable(row.today, tone, theme::MUTED);
    }
    for (int d = 0; d < days_; ++d) {
      const int x = cell_x_ + d * (cell_w_ + col_gap_);
      int bar_y;
      if (side_) {
        const int top = y + (row_h_ - body_h_ - ui::px(3) - bar_h_) / 2;
        place(row.word[d], x, top, cell_w_, body_h_);
        ui::set_text(row.word[d], it.step[d] < 0 ? "-" : level_text(it.step[d]));
        set_readable(row.word[d], tone_of(it.step[d]), it.step[d] > 0 ? theme::INK : theme::MUTED);
        bar_y = top + body_h_ + ui::px(3);
      } else {
        bar_y = y + title_h_ + ui::px(4);
      }
      bar(row.track[d], row.fill[d], x, bar_y, it.step[d]);
    }
  }

  // A bar from none to high: the track, and in it the part up to the level in the level's colour.
  void bar(lv_obj_t *track, lv_obj_t *fill, int x, int y, int step) {
    show(track, true);
    place(track, x, y, cell_w_, bar_h_);
    const bool some = step > 0;
    show(fill, some);
    place(fill, 0, 0, some ? std::max(bar_h_, cell_w_ * step / STEPS) : 1, bar_h_);
    ui::set_fill(fill, tone_of(step), theme::ACCENT);
  }

  void clear(Row &row) {
    ui::set_text(row.icon, "");
    ui::set_text(row.name, "");
    if (row.today) ui::set_text(row.today, "");
    for (int d = 0; d < days_; ++d) {
      if (row.word[d]) ui::set_text(row.word[d], "");
      show(row.track[d], false);
      show(row.fill[d], false);
    }
  }

  const Pollen *plugin_;
  int width_ = 0, height_ = 0, days_ = 2, rows_ = 0;
  int title_h_ = 0, body_h_ = 0, icon_h_ = 0, icon_w_ = 0, bar_h_ = 0, word_w_ = 0;
  int col_gap_ = 0, min_gap_ = 0, name_w_ = 0, cell_x_ = 0, cell_w_ = 0, row_h_ = 0;
  bool show_empty_ = false, sort_ = true, header_ = false, side_ = false;
  bool chosen_[ALLERGENS]{};
  lv_obj_t *head_[DAYS]{}, *big_{}, *note_{};
  Row row_[MAX_ROWS];
};

// ---- Circles: a ring per day that fills from none to high, as in the pollenprognos card ----


// A row per allergen: its name, and for each day a circle whose ring fills from none to high in green, amber or red,
// the allergen's icon in the middle in the same colour, and the level in words under it where there is room. The
// circles grow with the room: three allergens fill a tile two cells high. The strongest first; with nothing to show
// the tile says so with a large flower.
class CirclesTile : public tessera::Tile {
 public:
  explicit CirclesTile(const Pollen *plugin) : plugin_(plugin) {}

  void create(const tessera::TileContext &c) override {
    width_ = c.width;
    height_ = c.height;
    days_ = std::max(1, std::min(DAYS, atoi(c.options["days"] | "2")));
    show_empty_ = c.options["show_empty"] | false;
    sort_ = c.options["sort"] | true;
    show_words_ = strcmp(c.options["style"] | "circles", "circles_plain") != 0;
    for (int a = 0; a < ALLERGENS; ++a) {
      char key[24];
      snprintf(key, sizeof key, "show_%s", KEYS[a]);
      chosen_[a] = c.options[key] | true;
    }

    name_h_ = ui::line_height(Font::BODY_LARGE);
    body_h_ = ui::line_height(Font::BODY);
    gap_ = ui::px(ui::large() ? 10 : 6);
    min_d_ = ui::mm(8);
    // The name beside the circles, in a column of the longest name (two fifths of the tile at most), when every day's
    // cell keeps room for its circle and its words; else the name above its circles, which then share the whole width.
    // A day whose circle would be smaller than 8 mm is left out, the last day first.
    int longest = 0;
    for (const char *key : KEYS) longest = std::max(longest, ui::text_width(plugin_->text(key), Font::BODY_LARGE));
    for (int s = 0; s <= STEPS; ++s) word_w_ = std::max(word_w_, ui::text_width(level_text(s), Font::BODY));
    const int need = std::max(min_d_ + ui::px(4), show_words_ ? word_w_ + ui::px(4) : 0);
    name_w_ = std::min(longest + ui::px(10), width_ * 2 / 5);
    side_ = (width_ - name_w_ - (days_ - 1) * gap_) / days_ >= need;
    if (!side_) name_w_ = 0;
    for (;;) {
      cell_w_ = (width_ - name_w_ - (days_ - 1) * gap_) / days_;
      if (cell_w_ >= min_d_ + ui::px(4) || days_ == 1) break;
      --days_;
    }
    cell_w_ = std::max(1, cell_w_);
    header_ = days_ > 1;
    head_top_ = header_ ? body_h_ + ui::px(4) : 0;
    rows_ = std::max(1, std::min(MAX_ROWS, (height_ - head_top_ + gap_) / (min_d_ + (side_ ? 0 : name_h_) + gap_)));

    if (header_)
      for (int d = 0; d < days_; ++d) {
        head_[d] = ui::label(c.parent, Font::BODY, theme::MUTED);
        lv_obj_set_style_text_align(head_[d], LV_TEXT_ALIGN_CENTER, 0);
      }
    for (int r = 0; r < rows_; ++r) {
      Row &row = row_[r];
      row.name = ui::label(c.parent, Font::BODY_LARGE, theme::INK);
      for (int d = 0; d < days_; ++d) {
        Cell &cell = row.cell[d];
        cell.ring = make_ring(c.parent);
        cell.icon = ui::label(c.parent, Font::ICON_SMALL, theme::MUTED);
        lv_obj_set_style_text_align(cell.icon, LV_TEXT_ALIGN_CENTER, 0);
        cell.word = ui::label(c.parent, Font::BODY, theme::MUTED);
        lv_obj_set_style_text_align(cell.word, LV_TEXT_ALIGN_CENTER, 0);
      }
    }
    // What the tile shows with nothing to show: a large flower and a word under it.
    big_ = ui::label(c.parent, Font::ICON, theme::MUTED);
    lv_obj_set_style_text_align(big_, LV_TEXT_ALIGN_CENTER, 0);
    note_ = ui::label(c.parent, Font::BODY_LARGE, theme::MUTED);
    lv_obj_set_style_text_align(note_, LV_TEXT_ALIGN_CENTER, 0);
    const int ih = ui::line_height(Font::ICON), nh = ui::line_height(Font::BODY_LARGE);
    const int y = std::max(0, (height_ - ih - nh) / 2);
    place(big_, 0, y, width_, ih);
    place(note_, 0, y + ih, width_, nh);
  }

  void on_tick(uint32_t) override {
    // The allergens to show: chosen in the inspector, with a sensor, and (unless asked) with pollen on a shown day.
    Item items[ALLERGENS];
    int count = 0, with_sensor = 0;
    for (int a = 0; a < ALLERGENS; ++a) {
      if (!chosen_[a]) continue;
      Item it{a, {-1, -1, -1}, -1};
      for (int d = 0; d < days_; ++d) {
        it.step[d] = step_of(plugin_->level(a, d));
        it.most = std::max(it.most, it.step[d]);
      }
      if (it.most < 0) continue;  // no sensor, or it has no value
      ++with_sensor;
      if (!show_empty_ && it.most <= 0) continue;
      items[count++] = it;
    }
    if (sort_)
      std::stable_sort(items, items + count, [](const Item &x, const Item &y) {
        return x.step[0] != y.step[0] ? x.step[0] > y.step[0] : x.most > y.most;
      });

    const char *why = with_sensor == 0 ? "choose" : count == 0 ? "none" : nullptr;
    ui::set_text(big_, why ? ui::icon(ICON_FLOWER) : "");
    ui::set_tone(big_, why && *why == 'n' ? Tone::GOOD : Tone::NORMAL, theme::MUTED);
    ui::set_text(note_, why ? plugin_->text(why) : "");

    // As large as the room allows for the rows there are: the circle fits its cell and its share of the height, the
    // words under it only while the circle keeps at least 8 mm.
    const int shown = why ? 0 : std::min(count, rows_);
    const int room = height_ - head_top_;
    const int name_above = side_ ? 0 : name_h_;
    int share = shown ? (room - (shown - 1) * gap_) / shown - name_above : 0;
    bool words = show_words_ && share - body_h_ >= min_d_ && cell_w_ >= word_w_;
    int dia = std::min(std::min(cell_w_ - ui::px(4), ui::mm(18)), share - (words ? body_h_ + ui::px(2) : 0));
    dia = std::max(min_d_, dia);
    const int row_h = name_above + dia + (words ? body_h_ + ui::px(2) : 0);
    const int used = shown * row_h + std::max(0, shown - 1) * gap_;
    const int spare = std::max(0, room - used);
    // Rows a little apart when there is much room, and the block in the middle of what is left.
    const int extra = shown > 1 ? std::min(spare / (shown + 1), gap_ * 2) : 0;
    const int y0 = head_top_ + std::max(0, (spare - extra * (shown - 1)) / 2);

    for (int d = 0; d < days_; ++d)
      if (head_[d]) {
        ui::set_text(head_[d], shown ? plugin_->text(DAY_KEYS[d]) : "");
        place(head_[d], cell_x(d), std::max(0, y0 - head_top_), cell_w_, body_h_);
      }
    const bool big_icon = dia >= ui::line_height(Font::ICON) * 3 / 2;
    for (int r = 0; r < rows_; ++r) {
      if (r < shown) paint(row_[r], items[r], y0 + r * (row_h + gap_ + extra), dia, words, big_icon);
      else clear(row_[r]);
    }
  }

  void on_theme() override {
    for (int d = 0; d < days_; ++d)
      if (head_[d]) ui::set_color(head_[d], theme::MUTED);
    ui::set_color(note_, theme::MUTED);
    // The rings and icons take their colours on every tick.
  }

 private:
  struct Item {
    int allergen;
    int step[DAYS];
    int most;
  };
  struct Cell {
    lv_obj_t *ring{}, *icon{}, *word{};
  };
  struct Row {
    lv_obj_t *name{};
    Cell cell[DAYS];
  };

  int cell_x(int d) const { return name_w_ + d * (cell_w_ + gap_); }

  std::string level_text(int step) const {
    char key[8];
    snprintf(key, sizeof key, "lvl%d", std::max(0, step));
    return plugin_->text(key);
  }

  // A ring that fills clockwise from the top: an arc where the firmware has them, else a round outline.
  static lv_obj_t *make_ring(lv_obj_t *parent) {
#if LV_USE_ARC
    lv_obj_t *arc = lv_arc_create(parent);
    lv_obj_remove_style_all(arc);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_arc_set_rotation(arc, 270);
    lv_arc_set_bg_angles(arc, 0, 360);
    lv_arc_set_range(arc, 0, STEPS);
    lv_arc_set_value(arc, 0);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
    return arc;
#else
    lv_obj_t *ring = ui::block(parent, theme::CARD);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
    return ring;
#endif
  }

  void paint_ring(lv_obj_t *ring, int dia, int step) {
    const int w = std::max(ui::px(4), dia / 9);
    const Tone tone = tone_of(step);
#if LV_USE_ARC
    lv_obj_set_style_arc_width(ring, w, LV_PART_MAIN);
    lv_obj_set_style_arc_width(ring, w, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ring, ui::color(theme::TRACK), LV_PART_MAIN);
    lv_obj_set_style_arc_color(ring, ui::tone_color(tone, theme::ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(ring, step > 0 ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_arc_set_value(ring, std::max(0, step));
#else
    lv_obj_set_style_border_width(ring, w, 0);
    lv_obj_set_style_border_color(ring, step > 0 ? ui::tone_color(tone, theme::ACCENT) : ui::color(theme::TRACK), 0);
#endif
  }

  void paint(Row &row, const Item &it, int y, int dia, bool words, bool big_icon) {
    ui::set_text(row.name, plugin_->text(KEYS[it.allergen]));
    ui::set_color(row.name, theme::INK);
    if (side_) {
      place(row.name, 0, y + dia / 2 - name_h_ / 2, name_w_ - ui::px(6), name_h_);
    } else {
      place(row.name, 0, y, width_, name_h_);
      y += name_h_;
    }
    const int icon_y = y + dia / 2;
    const Font icon_font = big_icon ? Font::ICON : Font::ICON_SMALL;
    const int ih = ui::line_height(icon_font);
    for (int d = 0; d < days_; ++d) {
      Cell &cell = row.cell[d];
      const int s = it.step[d];
      const int x = cell_x(d) + (cell_w_ - dia) / 2;
      show(cell.ring, true);
      show(cell.icon, true);
      show(cell.word, true);
      place(cell.ring, x, y, dia, dia);
      paint_ring(cell.ring, dia, s);
      ui::set_font(cell.icon, icon_font);
      ui::set_text(cell.icon, ui::icon(ICONS[it.allergen]));
      set_readable(cell.icon, tone_of(s), theme::MUTED);
      place(cell.icon, x, icon_y - ih / 2, dia, ih);
      ui::set_text(cell.word, words ? (s < 0 ? std::string("-") : level_text(s)) : "");
      set_readable(cell.word, tone_of(s), theme::MUTED);
      place(cell.word, cell_x(d), y + dia + ui::px(2), cell_w_, body_h_);
    }
  }

  void clear(Row &row) {
    ui::set_text(row.name, "");
    for (int d = 0; d < days_; ++d) {
      show(row.cell[d].ring, false);
      show(row.cell[d].icon, false);
      show(row.cell[d].word, false);
    }
  }

  const Pollen *plugin_;
  int width_ = 0, height_ = 0, days_ = 2, rows_ = 0;
  int name_h_ = 0, body_h_ = 0, gap_ = 0, min_d_ = 0, name_w_ = 0, cell_w_ = 0, head_top_ = 0, word_w_ = 0;
  bool show_empty_ = false, sort_ = true, show_words_ = true, header_ = false, side_ = true;
  bool chosen_[ALLERGENS]{};
  lv_obj_t *head_[DAYS]{}, *big_{}, *note_{};
  Row row_[MAX_ROWS];
};

// ---- The tile: bars or circles, as its inspector says; a tap opens the card ----

class ForecastTile : public tessera::Tile {
 public:
  explicit ForecastTile(const Pollen *plugin) : plugin_(plugin) {}
  ~ForecastTile() override { delete inner_; }

  void create(const tessera::TileContext &c) override {
    tile_ = c.tile;
    const char *style = c.options["style"] | "circles";
    if (strcmp(style, "bars") == 0) inner_ = new BarsTile(plugin_);
    else inner_ = new CirclesTile(plugin_);
    inner_->create(c);
  }
  void on_state(JsonObjectConst data) override { inner_->on_state(data); }
  void on_tick(uint32_t epoch) override { inner_->on_tick(epoch); }
  void on_theme() override { inner_->on_theme(); }
  void on_tap() override { tessera::open_card(plugin_->plugin_id(), "detail", "", tile_); }

 private:
  const Pollen *plugin_;
  tessera::Tile *inner_{};
  int tile_ = -1;
};

// ---- The card: every allergen with a sensor, three days, as circles with words ----

class DetailCard : public tessera::Card {
 public:
  explicit DetailCard(const Pollen *plugin) : circles_(plugin) {}

  void open(const tessera::CardContext &c) override {
    JsonDocument options;
    options["days"] = "3";
    options["show_empty"] = true;
    options["sort"] = true;
    options["style"] = "circles";
    tessera::TileContext t{};
    t.parent = c.parent;
    t.width = c.width;
    t.height = c.height;
    t.columns = 4;
    t.rows = 4;
    t.name = "";
    t.entity = "";
    t.tile = -1;
    t.options = options.as<JsonObjectConst>();
    circles_.create(t);
    circles_.on_tick(0);
  }
  void on_tick(uint32_t epoch) override { circles_.on_tick(epoch); }
  void on_theme() override { circles_.on_theme(); }

 private:
  CirclesTile circles_;
};

// ---- The plugin ----

void Pollen::setup() {
  add_tile("forecast", [this]() { return new ForecastTile(this); });
  add_card("detail", [this]() { return new DetailCard(this); });
  add_bar_item("alert", [this]() { return bar(); });
}

// "Birch: high" in the top bar while the strongest allergen of today reaches the level set on the screen.
tessera::BarItem Pollen::bar() const {
  tessera::BarItem item;
  if (in_bar_ && !in_bar_->state) return item;
  const int from = alert_level_ && !std::isnan(alert_level_->state) ? static_cast<int>(alert_level_->state) : 5;
  int best = -1, step = 0;
  for (int a = 0; a < ALLERGENS; ++a) {
    const int s = step_of(level(a, 0));
    if (s > step) {
      step = s;
      best = a;
    }
  }
  if (best < 0 || step < std::max(1, from)) return item;
  char key[8];
  snprintf(key, sizeof key, "lvl%d", step);
  item.shown = true;
  item.icon = ICON_FLOWER;
  item.tone = tone_of(step);
  item.text = tessera::fill(tessera::fill(text("bar_alert"), "name", text(KEYS[best])).c_str(), "level", text(key));
  return item;
}

bool Pollen::settings(tessera::SettingsPage &page) {
  page.icon = "\U000F024A";
  page.toggle(text("in_bar"), [this]() { return !in_bar_ || in_bar_->state; },
              [this](bool on) { if (in_bar_) { if (on) in_bar_->turn_on(); else in_bar_->turn_off(); } });
  page.number(text("alert_level"), 1, STEPS, 1, "",
              [this]() { return alert_level_ && !std::isnan(alert_level_->state) ? static_cast<int>(alert_level_->state) : 5; },
              [this](int value) {
                if (!alert_level_) return;
                auto call = alert_level_->make_call();
                call.set_value(value);
                call.perform();
              });
  page.card(text("open_card"), "\U000F024A", "detail");
  return true;
}

}  // namespace esphome::pollen
