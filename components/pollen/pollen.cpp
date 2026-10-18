#include "pollen.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace esphome::pollen {

using tessera::Font;
using tessera::Tone;
namespace ui = tessera::ui;

// The allergens' keys: option show_<key>, screen texts <key> (the name) and the order of the sensors.
static const char *const KEYS[ALLERGENS] = {"erle", "hasel", "esche", "birke", "graeser", "roggen", "beifuss", "ambrosia"};
// Their icons: trees, grasses and herbs.
static const uint32_t ICONS[ALLERGENS] = {0xF0531, 0xF0531, 0xF0531, 0xF0531, 0xF1510, 0xF1510, 0xF0E66, 0xF0E66};
static const char *const DAY_KEYS[DAYS] = {"today", "tomorrow", "in_2_days"};
static constexpr int MAX_ROWS = ALLERGENS;

// DWD's level 0 to 3 in steps of 0.5 as a step from 0 (none) to 6 (high); -1 for none known.
static int step_of(float level) {
  if (std::isnan(level) || level < 0) return -1;
  return std::min(6, static_cast<int>(level * 2 + 0.25f));
}

// The colour of a step: nothing, then green for low, amber for moderate, red for high.
static Tone tone_of(int step) {
  if (step <= 0) return Tone::NORMAL;
  if (step <= 2) return Tone::GOOD;
  if (step <= 4) return Tone::BUSY;
  return Tone::ALERT;
}

// ---- The tile ----

// A row per allergen: its icon and name, then for each day a dot in the level's colour and, where there is room, the
// level in words ("moderate"). Above the days their names, when there is room for a header. The strongest allergens
// come first; the rows that do not fit are the weakest ones.
class ForecastTile : public tessera::Tile {
 public:
  explicit ForecastTile(const Pollen *plugin) : plugin_(plugin) {}

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

    const int title_h = ui::line_height(Font::BODY_LARGE), body_h = ui::line_height(Font::BODY);
    gap_ = ui::px(ui::large() ? 8 : 4);
    dot_ = std::max(ui::px(10), title_h * 2 / 3);
    row_h_ = std::max(title_h, dot_);
    // A header with the days only when it leaves room for two rows; a single day needs none.
    header_ = days_ > 1 && body_h + gap_ + 2 * row_h_ + gap_ <= height_;
    const int top = header_ ? body_h + gap_ : 0;
    rows_ = std::max(1, std::min(MAX_ROWS, (height_ - top + gap_) / (row_h_ + gap_)));
    // The name's column: the longest name, or two fifths of the tile when that is less.
    int longest = 0;
    for (const char *key : KEYS) longest = std::max(longest, ui::text_width(plugin_->text(key), Font::BODY_LARGE));
    icon_w_ = width_ >= ui::mm(30) ? ui::line_height(Font::ICON_SMALL) + ui::px(4) : 0;
    name_w_ = std::min(icon_w_ + longest + ui::px(8), width_ * 2 / 5);
    cell_w_ = std::max(1, (width_ - name_w_) / days_);
    // The level in words beside the dot, when the widest word fits.
    int widest = 0;
    for (int s = 0; s <= 6; ++s) widest = std::max(widest, ui::text_width(level_text(s), Font::BODY));
    words_ = cell_w_ >= dot_ + ui::px(6) + widest;

    if (header_) {
      for (int d = 0; d < days_; ++d) {
        head_[d] = ui::label(c.parent, Font::BODY, theme::MUTED);
        lv_obj_set_size(head_[d], cell_w_, body_h);
        lv_obj_set_pos(head_[d], name_w_ + d * cell_w_, 0);
        ui::set_text(head_[d], plugin_->text(DAY_KEYS[d]));
      }
    }
    for (int r = 0; r < rows_; ++r) {
      Row &row = row_[r];
      const int y = top + r * (row_h_ + gap_);
      if (icon_w_) {
        row.icon = ui::label(c.parent, Font::ICON_SMALL, theme::MUTED);
        place(row.icon, 0, y + (row_h_ - ui::line_height(Font::ICON_SMALL)) / 2, icon_w_,
              ui::line_height(Font::ICON_SMALL));
      }
      row.name = ui::label(c.parent, Font::BODY_LARGE, theme::INK);
      place(row.name, icon_w_, y + (row_h_ - title_h) / 2, name_w_ - icon_w_ - ui::px(4), title_h);
      for (int d = 0; d < days_; ++d) {
        const int x = name_w_ + d * cell_w_;
        row.dot[d] = ui::block(c.parent, theme::TRACK);
        lv_obj_set_style_radius(row.dot[d], LV_RADIUS_CIRCLE, 0);
        place(row.dot[d], x, y + (row_h_ - dot_) / 2, dot_, dot_);
        if (words_) {
          row.word[d] = ui::label(c.parent, Font::BODY, theme::MUTED);
          place(row.word[d], x + dot_ + ui::px(6), y + (row_h_ - body_h) / 2, cell_w_ - dot_ - ui::px(8), body_h);
        }
      }
    }
    note_ = ui::label(c.parent, Font::BODY_LARGE, theme::MUTED);
    lv_obj_set_style_text_align(note_, LV_TEXT_ALIGN_CENTER, 0);
    place(note_, 0, (height_ - title_h) / 2, width_, title_h);
  }

  void on_tick(uint32_t) override {
    // The allergens to show: chosen in the inspector, with a sensor, and (unless asked) with pollen on a shown day.
    struct Item {
      int allergen;
      int step[DAYS];
      int most;
    } items[ALLERGENS];
    int count = 0, with_sensor = 0;
    for (int a = 0; a < ALLERGENS; ++a) {
      if (!chosen_[a]) continue;
      Item it{a, {-1, -1, -1}, -1};
      for (int d = 0; d < days_; ++d) {
        it.step[d] = step_of(plugin_->level(a, d));
        it.most = std::max(it.most, it.step[d]);
      }
      if (it.step[0] < 0 && it.most < 0) continue;  // no sensor, or it has no value
      ++with_sensor;
      if (!show_empty_ && it.most <= 0) continue;
      items[count++] = it;
    }
    if (sort_)
      std::stable_sort(items, items + count, [](const Item &x, const Item &y) {
        return x.step[0] != y.step[0] ? x.step[0] > y.step[0] : x.most > y.most;
      });

    const char *why = with_sensor == 0 ? "choose" : count == 0 ? "none" : nullptr;
    ui::set_text(note_, why ? plugin_->text(why) : "");
    for (int d = 0; d < days_; ++d)
      if (head_[d]) ui::set_text(head_[d], why ? "" : plugin_->text(DAY_KEYS[d]));
    for (int r = 0; r < rows_; ++r) paint(row_[r], why || r >= count ? nullptr : &items[r].allergen, items[r].step);
  }

  void on_theme() override {
    for (int d = 0; d < days_; ++d)
      if (head_[d]) ui::set_color(head_[d], theme::MUTED);
    ui::set_color(note_, theme::MUTED);
    // The rows take their colours on every tick.
  }

 private:
  struct Row {
    lv_obj_t *icon{}, *name{};
    lv_obj_t *dot[DAYS]{}, *word[DAYS]{};
  };

  std::string level_text(int step) const {
    char key[8];
    snprintf(key, sizeof key, "lvl%d", std::max(0, step));
    return plugin_->text(key);
  }

  // One row: an allergen, or nothing (allergen null) for a row below the last one.
  void paint(Row &row, const int *allergen, const int *step) {
    const bool shown = allergen != nullptr;
    if (row.icon) {
      ui::set_text(row.icon, shown ? ui::icon(ICONS[*allergen]) : "");
      ui::set_color(row.icon, theme::MUTED);
    }
    ui::set_text(row.name, shown ? plugin_->text(KEYS[*allergen]) : "");
    ui::set_color(row.name, theme::INK);
    for (int d = 0; d < days_; ++d) {
      const int s = shown ? step[d] : -1;
      lv_obj_set_style_opa(row.dot[d], shown ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
      ui::set_fill(row.dot[d], tone_of(s), theme::TRACK);
      if (row.word[d]) {
        ui::set_text(row.word[d], !shown ? "" : s < 0 ? "-" : level_text(s));
        ui::set_color(row.word[d], s >= 3 ? theme::INK : theme::MUTED);
      }
    }
  }

  static void place(lv_obj_t *obj, int x, int y, int w, int h) {
    lv_obj_set_size(obj, std::max(1, w), std::max(1, h));
    lv_obj_set_pos(obj, x, y);
  }

  const Pollen *plugin_;
  int width_ = 0, height_ = 0, days_ = 2, rows_ = 0;
  int gap_ = 0, dot_ = 0, row_h_ = 0, icon_w_ = 0, name_w_ = 0, cell_w_ = 0;
  bool show_empty_ = false, sort_ = true, header_ = false, words_ = false;
  bool chosen_[ALLERGENS]{};
  lv_obj_t *head_[DAYS]{}, *note_{};
  Row row_[MAX_ROWS];
};

void Pollen::setup() {
  add_tile("forecast", [this]() { return new ForecastTile(this); });
}

}  // namespace esphome::pollen
