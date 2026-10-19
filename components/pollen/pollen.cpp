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

// ---- The tile ----

// A block per allergen: its icon in the colour of today's level and its name, and for each day a bar that fills from
// none to high in green, amber or red, with the level in words. On a narrow tile the bars sit under the name, on a wide
// one beside it. The blocks spread over the tile's height, the strongest first; with nothing to show the tile says so
// with a large flower.
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
    ui::set_tone(row.icon, tone, theme::MUTED);
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
      ui::set_tone(row.today, tone, theme::MUTED);
    }
    for (int d = 0; d < days_; ++d) {
      const int x = cell_x_ + d * (cell_w_ + col_gap_);
      int bar_y;
      if (side_) {
        const int top = y + (row_h_ - body_h_ - ui::px(3) - bar_h_) / 2;
        place(row.word[d], x, top, cell_w_, body_h_);
        ui::set_text(row.word[d], it.step[d] < 0 ? "-" : level_text(it.step[d]));
        ui::set_tone(row.word[d], tone_of(it.step[d]), it.step[d] > 0 ? theme::INK : theme::MUTED);
        bar_y = top + body_h_ + ui::px(3);
      } else {
        bar_y = y + title_h_ + ui::px(4);
      }
      bar(row.track[d], row.fill[d], x, bar_y, it.step[d]);
    }
  }

  // A bar from none to high: the track, and in it the part up to the level in the level's colour.
  void bar(lv_obj_t *track, lv_obj_t *fill, int x, int y, int step) {
    lv_obj_set_style_opa(track, LV_OPA_COVER, 0);
    place(track, x, y, cell_w_, bar_h_);
    const bool some = step > 0;
    lv_obj_set_style_opa(fill, some ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    place(fill, 0, 0, some ? std::max(bar_h_, cell_w_ * step / STEPS) : 1, bar_h_);
    ui::set_fill(fill, tone_of(step), theme::ACCENT);
  }

  void clear(Row &row) {
    ui::set_text(row.icon, "");
    ui::set_text(row.name, "");
    if (row.today) ui::set_text(row.today, "");
    for (int d = 0; d < days_; ++d) {
      if (row.word[d]) ui::set_text(row.word[d], "");
      lv_obj_set_style_opa(row.track[d], LV_OPA_TRANSP, 0);
      lv_obj_set_style_opa(row.fill[d], LV_OPA_TRANSP, 0);
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

void Pollen::setup() {
  add_tile("forecast", [this]() { return new ForecastTile(this); });
}

}  // namespace esphome::pollen
