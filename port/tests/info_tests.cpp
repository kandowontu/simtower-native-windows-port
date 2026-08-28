#include "original_dib.hpp"
#include "original_dtmp.hpp"
#include "original_finance.hpp"
#include "original_info.hpp"
#include "original_resources.hpp"
#include "original_tdt.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <vector>

namespace {

struct TestDibSurface {
  HDC dc{};
  HBITMAP bitmap{};
  HGDIOBJ previous{};
  std::uint32_t* pixels{};
  int width{};
  int height{};

  TestDibSurface(int surface_width, int surface_height)
      : width(surface_width), height(surface_height) {
    dc = CreateCompatibleDC(nullptr);
    assert(dc);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1U;
    info.bmiHeader.biBitCount = 32U;
    info.bmiHeader.biCompression = BI_RGB;
    void* storage{};
    bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &storage,
                              nullptr, 0U);
    assert(bitmap && storage);
    pixels = static_cast<std::uint32_t*>(storage);
    previous = SelectObject(dc, bitmap);
  }

  ~TestDibSurface() {
    if (previous) SelectObject(dc, previous);
    if (bitmap) DeleteObject(bitmap);
    if (dc) DeleteDC(dc);
  }

  TestDibSurface(const TestDibSurface&) = delete;
  TestDibSurface& operator=(const TestDibSurface&) = delete;
};

std::vector<std::byte> read_file(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  assert(stream);
  std::vector<char> chars((std::istreambuf_iterator<char>(stream)),
                          std::istreambuf_iterator<char>());
  std::vector<std::byte> bytes(chars.size());
  for (std::size_t i = 0; i < chars.size(); ++i) {
    bytes[i] = static_cast<std::byte>(static_cast<unsigned char>(chars[i]));
  }
  return bytes;
}

}  // namespace

int main(int argc, char** argv) {
  {
    // Direct 1060:0083 modal-launch wrapper coverage.
    assert(simtower::original_finance_launcher_contract() ==
           (simtower::OriginalFinanceLauncherContract{500U, true, false}));
    // COUNTDLOGMAIN 1060:02a4-037e visibly presses and releases its custom
    // button before closing for Return/Space; unrelated key-up is ignored.
    using Action = simtower::OriginalFinanceDialogMessageAction;
    using MessagePlan = simtower::OriginalFinanceDialogMessagePlan;
    using Presentation = simtower::OriginalFinanceKeyPresentation;
    assert(simtower::original_finance_dialog_message_plan(0x000fU) ==
           (MessagePlan{Action::paint, false}));
    assert(simtower::original_finance_dialog_message_plan(0x0019U) ==
           (MessagePlan{Action::control_color, false}));
    assert(simtower::original_finance_dialog_message_plan(0x0101U) ==
           (MessagePlan{Action::key_up, false}));
    assert(simtower::original_finance_dialog_message_plan(0x0110U) ==
           (MessagePlan{Action::initialize, true}));
    assert(simtower::original_finance_dialog_message_plan(0x0201U) ==
           (MessagePlan{Action::left_button_down, false}));
    assert(simtower::original_finance_dialog_message_plan(0x0202U) ==
           (MessagePlan{Action::left_button_up, false}));
    assert(simtower::original_finance_dialog_message_plan(0x0010U) ==
           (MessagePlan{Action::unhandled, false}));
    assert(simtower::original_finance_key_presentation(0x0dU) ==
           (Presentation{true, true, true}));
    assert(simtower::original_finance_key_presentation(0x20U) ==
           (Presentation{true, true, true}));
    assert(simtower::original_finance_key_presentation(0x1bU) ==
           (Presentation{false, false, false}));
    // Direct 11e0:00ca and 1208:0c89/07a5 formatter-placement coverage after
    // text measurement.
    using ValuePosition = simtower::OriginalFinanceValuePosition;
    assert(simtower::original_finance_value_position(6U, 100, 20, 17) ==
           (ValuePosition{83, 21}));
    assert(simtower::original_finance_value_position(8U, 100, 20, 17) ==
           (ValuePosition{143, 22}));
    assert(simtower::original_finance_value_position(9U, 100, 20, 17) ==
           (ValuePosition{143, 22}));
    assert(simtower::original_finance_value_position(10U, 100, 20, 17) ==
           (ValuePosition{147, 21}));
    assert(simtower::original_finance_value_position(14U, 100, 20, 17) ==
           (ValuePosition{147, 21}));
    assert(simtower::original_finance_value_position(15U, 100, 20, 17) ==
           (ValuePosition{83, 21}));
  }

  assert(argc == 2);
  const auto pack = read_file(argv[1]);
  const simtower::OriginalResources resources(pack);

  // Direct 1128:13fc coverage: startup's Info backing is the complete
  // 431x41 BITMAP/320 surface; the four adjoining resources are the exact
  // rating, clock, and special-rating overlays consumed by its painters.
  const struct {
    int id;
    int width;
    int height;
  } bitmaps[] = {{320, 431, 41}, {321, 32, 31}, {322, 24, 19},
                 {323, 24, 19},  {327, 108, 22}};
  for (const auto& expected : bitmaps) {
    const auto dib = simtower::original_dib_view(
        resources.find("BITMAP", expected.id));
    assert(dib.width == expected.width);
    assert(dib.height == expected.height);
    assert(dib.bit_count == 8U);
  }

  {
    // Direct 1118:045d raster coverage. 1118:0ba4's 152x16 date field is
    // offset to client (151,13), filled with 0x999999, and leaves its
    // right/bottom edges excluded. Weekends emit STRL/713 item 10 in dark
    // red before returning to 0x262626 for the quarter/year suffix.
    constexpr int width = simtower::kOriginalInfoWidth;
    constexpr int height = simtower::kOriginalInfoClientTop +
                           simtower::kOriginalInfoBackingHeight;
    TestDibSurface base(width, height);
    TestDibSurface weekday(width, height);
    TestDibSurface weekend(width, height);
    simtower::draw_original_dib(
        base.dc, resources.find("BITMAP", 320), 0,
        simtower::kOriginalInfoClientTop);
    simtower::draw_original_info(weekday.dc, resources, nullptr);

    const auto at = [width](const TestDibSurface& surface, int x, int y) {
      return surface.pixels[static_cast<std::size_t>(y * width + x)];
    };
    assert(at(weekday, 151, 13) == 0x00999999U);
    assert(at(weekday, 302, 13) == 0x00999999U);
    assert(at(weekday, 150, 13) == at(base, 150, 13));
    assert(at(weekday, 151, 29) == at(base, 151, 29));
    std::size_t weekday_dark{};
    for (int y = 13; y < 29; ++y) {
      for (int x = 151; x < 303; ++x) {
        if (at(weekday, x, y) == 0x00262626U) ++weekday_dark;
      }
    }
    assert(weekday_dark != 0U);

    auto weekend_tower = simtower::make_original_new_tdt();
    weekend_tower.header.current_day = 2;
    simtower::draw_original_info(
        weekend.dc, resources, &weekend_tower);
    std::size_t weekend_red{};
    std::size_t weekend_dark{};
    for (int y = 13; y < 29; ++y) {
      for (int x = 151; x < 303; ++x) {
        if (at(weekend, x, y) == 0x00b30000U) ++weekend_red;
        if (at(weekend, x, y) == 0x00262626U) ++weekend_dark;
      }
    }
    assert(weekend_red != 0U && weekend_dark != 0U);
  }

  {
    // Direct 1118:0044/0b67 and 1208:0a42 raster coverage. Ratings one through
    // five repaint only the five clipped 21x19 BITMAP/322-or-323 star cells at
    // [43,148)x[11,30). Rating six instead clips BITMAP/327 to the special
    // [42,148)x[10,32) strip.
    constexpr int width = simtower::kOriginalInfoWidth;
    constexpr int height = simtower::kOriginalInfoClientTop +
                           simtower::kOriginalInfoBackingHeight;
    TestDibSurface rating_one(width, height);
    TestDibSurface rating_five(width, height);
    TestDibSurface rating_six(width, height);
    auto tower = simtower::make_original_new_tdt();
    tower.header.rating = 1U;
    simtower::draw_original_info(rating_one.dc, resources, &tower);
    tower.header.rating = 5U;
    simtower::draw_original_info(rating_five.dc, resources, &tower);
    tower.header.rating = 6U;
    simtower::draw_original_info(rating_six.dc, resources, &tower);

    const auto at = [width](const TestDibSurface& surface, int x, int y) {
      return surface.pixels[static_cast<std::size_t>(y * width + x)];
    };
    bool ordinary_changed{};
    bool special_changed{};
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        if (at(rating_one, x, y) != at(rating_five, x, y)) {
          assert(x >= 43 && x < 148 && y >= 11 && y < 30);
          ordinary_changed = true;
        }
        if (at(rating_five, x, y) != at(rating_six, x, y)) {
          assert(x >= 42 && x < 148 && y >= 10 && y < 32);
          special_changed = true;
        }
      }
    }
    assert(ordinary_changed && special_changed);
  }

  {
    // Direct 1118:0143 raster coverage: 1118:0bda's 70x14 balance rectangle
    // is offset by (10,8), filled with 0x999999, and receives right/baseline-
    // aligned font-13 text at its inclusive bottom-right pixel. Changing only
    // b3ce must therefore alter pixels solely inside client [354,424)x[13,27).
    constexpr int width = simtower::kOriginalInfoWidth;
    constexpr int height = simtower::kOriginalInfoClientTop +
                           simtower::kOriginalInfoBackingHeight;
    TestDibSurface positive(width, height);
    TestDibSurface negative(width, height);
    auto tower = simtower::make_original_new_tdt();
    tower.header.balance = 123;
    simtower::draw_original_info(positive.dc, resources, &tower);
    tower.header.balance = -1;
    simtower::draw_original_info(negative.dc, resources, &tower);

    const auto at = [width](const TestDibSurface& surface, int x, int y) {
      return surface.pixels[static_cast<std::size_t>(y * width + x)];
    };
    assert(at(positive, 354, 13) == 0x00999999U);
    assert(at(positive, 423, 13) == 0x00999999U);
    bool changed_inside{};
    std::size_t positive_dark{};
    std::size_t negative_dark{};
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        const bool inside = x >= 354 && x < 424 && y >= 13 && y < 27;
        if (at(positive, x, y) != at(negative, x, y)) {
          assert(inside);
          changed_inside = true;
        }
        if (inside && at(positive, x, y) == 0x00262626U) ++positive_dark;
        if (inside && at(negative, x, y) == 0x00262626U) ++negative_dark;
      }
    }
    assert(changed_inside && positive_dark != 0U && negative_dark != 0U);
  }

  {
    // Direct 1118:026a raster coverage. 1118:0c0f supplies a 262x11 field at
    // (41,25); the painter offsets it by eight client pixels, fills 0xd9d9d9,
    // adds the one-pixel white lower edge, and clips font-12 status text to the
    // resulting client [41,303)x[33,44) rectangle.
    constexpr int width = simtower::kOriginalInfoWidth;
    constexpr int height = simtower::kOriginalInfoClientTop +
                           simtower::kOriginalInfoBackingHeight;
    TestDibSurface empty(width, height);
    TestDibSurface message(width, height);
    simtower::draw_original_info(empty.dc, resources, nullptr, {});
    simtower::draw_original_info(message.dc, resources, nullptr,
                                 "Construction status");

    const auto at = [width](const TestDibSurface& surface, int x, int y) {
      return surface.pixels[static_cast<std::size_t>(y * width + x)];
    };
    assert(at(empty, 41, 33) == 0x00d9d9d9U);
    assert(at(empty, 302, 43) == 0x00d9d9d9U);
    assert(at(empty, 41, 44) == 0x00ffffffU);
    assert(at(empty, 302, 44) == 0x00ffffffU);
    bool changed_inside{};
    std::size_t dark_pixels{};
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        const bool inside = x >= 41 && x < 303 && y >= 33 && y < 44;
        if (at(empty, x, y) != at(message, x, y)) {
          assert(inside);
          changed_inside = true;
        }
        if (inside && at(message, x, y) == 0x00262626U) ++dark_pixels;
      }
    }
    assert(changed_inside && dark_pixels != 0U);
  }

  {
    // Direct 1118:0368 raster coverage. 1118:0c44 supplies the 86x14 field at
    // client [337,423)x[31,45); population text is right-aligned by its
    // measured GDI extent at (right-extent,bottom-2).
    constexpr int width = simtower::kOriginalInfoWidth;
    constexpr int height = simtower::kOriginalInfoClientTop +
                           simtower::kOriginalInfoBackingHeight;
    TestDibSurface one(width, height);
    TestDibSurface many(width, height);
    auto tower = simtower::make_original_new_tdt();
    tower.post_elevator.finance.total_population = 1;
    simtower::draw_original_info(one.dc, resources, &tower);
    tower.post_elevator.finance.total_population = 123456;
    simtower::draw_original_info(many.dc, resources, &tower);

    const auto at = [width](const TestDibSurface& surface, int x, int y) {
      return surface.pixels[static_cast<std::size_t>(y * width + x)];
    };
    assert(at(one, 337, 31) == 0x00999999U);
    assert(at(one, 337, 44) == 0x00999999U);
    bool changed_inside{};
    std::size_t one_dark{};
    std::size_t many_dark{};
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        const bool inside = x >= 337 && x < 423 && y >= 31 && y < 45;
        if (at(one, x, y) != at(many, x, y)) {
          assert(inside);
          changed_inside = true;
        }
        if (inside && at(one, x, y) == 0x00262626U) ++one_dark;
        if (inside && at(many, x, y) == 0x00262626U) ++many_dark;
      }
    }
    assert(changed_inside && one_dark != 0U && many_dark != 0U);
  }

  {
    // Direct 1118:073d/0c79/0cad and 1208:0d35 raster coverage. The retained
    // 31x31 clock face is presented at client [5,36)x[13,44); changing only
    // b3de therefore moves
    // the two table-derived black hands without touching any pixel outside it.
    constexpr int width = simtower::kOriginalInfoWidth;
    constexpr int height = simtower::kOriginalInfoClientTop +
                           simtower::kOriginalInfoBackingHeight;
    TestDibSurface morning(width, height);
    TestDibSurface afternoon(width, height);
    auto tower = simtower::make_original_new_tdt();
    tower.header.frame_time = 0U;
    simtower::draw_original_info(morning.dc, resources, &tower);
    tower.header.frame_time = 1200U;
    simtower::draw_original_info(afternoon.dc, resources, &tower);

    const auto at = [width](const TestDibSurface& surface, int x, int y) {
      return surface.pixels[static_cast<std::size_t>(y * width + x)];
    };
    bool changed_inside{};
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        const bool inside = x >= 5 && x < 36 && y >= 13 && y < 44;
        if (at(morning, x, y) != at(afternoon, x, y)) {
          assert(inside);
          changed_inside = true;
        }
      }
    }
    assert(changed_inside);
  }

  for (const int id : {500, 501}) {
    const auto dib = simtower::original_dib_view(
        resources.find("BITMAP", id));
    assert(dib.width == 343);
    assert(dib.height == 364);
    assert(dib.bit_count == 8U);
  }
  // Direct COUNTDLOGMAIN 1060:00d3 coverage: its custom Finance surface is
  // BITMAP/500, DTMP/500 rectangle one is swapped from BITMAP/501 while the
  // mouse is held, and 0479 derives the displayed table and summary values.
  // The resource/raster/data assertions below exercise those paths without
  // opening the modal dialog.
  const auto finance_dtmp = simtower::parse_original_dtmp(
      resources.find("DTMP", 500));
  assert(finance_dtmp.bitmap_reference == "500");
  assert(finance_dtmp.width_or_header == 0U);
  assert(finance_dtmp.height_or_header == 0U);
  assert(finance_dtmp.rectangles ==
         std::vector<simtower::OriginalDtmpRect>({
             {131, 326, 223, 347}, {32, 335, 96, 349},
             {87, 78, 133, 210},   {133, 78, 189, 210},
             {268, 78, 324, 210},  {72, 40, 153, 54},
             {225, 40, 306, 54},   {98, 7, 121, 21},
             {170, 7, 193, 21},    {165, 232, 246, 246},
             {165, 249, 246, 263}, {165, 266, 246, 280},
             {165, 283, 246, 297}, {165, 300, 246, 314},
         }));

  // The partial DIB path used for the Finance pressed state must preserve
  // every BITMAP/500 pixel outside DTMP rectangle one and reproduce the same
  // BITMAP/501 pixels inside it. This is a memory-DC test; no window opens.
  TestDibSurface finance_base(343, 364);
  TestDibSurface finance_pressed(343, 364);
  TestDibSurface finance_composite(343, 364);
  simtower::draw_original_dib(
      finance_base.dc, resources.find("BITMAP", 500), 0, 0);
  simtower::draw_original_dib(
      finance_pressed.dc, resources.find("BITMAP", 501), 0, 0);
  simtower::draw_original_dib(
      finance_composite.dc, resources.find("BITMAP", 500), 0, 0);
  const auto& finance_button = finance_dtmp.rectangles[0];
  simtower::draw_original_dib_region(
      finance_composite.dc, resources.find("BITMAP", 501),
      finance_button.left, finance_button.top,
      finance_button.left, finance_button.top,
      finance_button.right - finance_button.left,
      finance_button.bottom - finance_button.top);
  bool finance_button_changed = false;
  for (int y = 0; y < 364; ++y) {
    for (int x = 0; x < 343; ++x) {
      const auto index = static_cast<std::size_t>(y * 343 + x);
      const bool inside = x >= finance_button.left &&
                          x < finance_button.right &&
                          y >= finance_button.top &&
                          y < finance_button.bottom;
      if (inside) {
        assert(finance_composite.pixels[index] ==
               finance_pressed.pixels[index]);
        finance_button_changed = finance_button_changed ||
            finance_base.pixels[index] != finance_pressed.pixels[index];
      } else {
        assert(finance_composite.pixels[index] == finance_base.pixels[index]);
      }
    }
  }
  assert(finance_button_changed);

  // Direct 1118:0ce7 coverage: values outside 10..19 use signed IDIV by ten
  // and select STRL/713 entries 11..13 only for positive remainders 1..3;
  // the teens and every other remainder use entry 14.
  assert(simtower::original_info_ordinal(resources, -1) == "-1th ");
  assert(simtower::original_info_ordinal(resources, 0) == "0th ");
  assert(simtower::original_info_ordinal(resources, 1) == "1st ");
  assert(simtower::original_info_ordinal(resources, 2) == "2nd ");
  assert(simtower::original_info_ordinal(resources, 3) == "3rd ");
  assert(simtower::original_info_ordinal(resources, 4) == "4th ");
  assert(simtower::original_info_ordinal(resources, 11) == "11th ");
  assert(simtower::original_info_ordinal(resources, 12) == "12th ");
  assert(simtower::original_info_ordinal(resources, 13) == "13th ");
  assert(simtower::original_info_ordinal(resources, 19) == "19th ");
  assert(simtower::original_info_ordinal(resources, 20) == "20th ");
  assert(simtower::original_info_ordinal(resources, 21) == "21st ");
  assert(simtower::original_info_ordinal(resources, 101) == "101st ");

  // Direct 1200:058d coverage: exercise every phase switch and each adjoining
  // 400-tick boundary, including the two half-speed midnight phases and the
  // final twelve-hour sweep.
  assert((simtower::original_info_clock_time(0) ==
          simtower::OriginalInfoClockTime{7, 0}));
  assert((simtower::original_info_clock_time(399) ==
          simtower::OriginalInfoClockTime{11, 59}));
  assert((simtower::original_info_clock_time(400) ==
          simtower::OriginalInfoClockTime{0, 0}));
  assert((simtower::original_info_clock_time(799) ==
          simtower::OriginalInfoClockTime{0, 29}));
  assert((simtower::original_info_clock_time(800) ==
          simtower::OriginalInfoClockTime{0, 30}));
  assert((simtower::original_info_clock_time(1199) ==
          simtower::OriginalInfoClockTime{0, 59}));
  assert((simtower::original_info_clock_time(1200) ==
          simtower::OriginalInfoClockTime{1, 0}));
  assert((simtower::original_info_clock_time(1599) ==
          simtower::OriginalInfoClockTime{4, 59}));
  assert((simtower::original_info_clock_time(1600) ==
          simtower::OriginalInfoClockTime{5, 0}));
  assert((simtower::original_info_clock_time(1999) ==
          simtower::OriginalInfoClockTime{8, 59}));
  assert((simtower::original_info_clock_time(2000) ==
          simtower::OriginalInfoClockTime{9, 0}));
  assert((simtower::original_info_clock_time(2399) ==
          simtower::OriginalInfoClockTime{0, 59}));
  assert((simtower::original_info_clock_time(2400) ==
          simtower::OriginalInfoClockTime{1, 0}));

  // Direct 1200:0037 and 1000:0f1c/1049 table/runtime-helper coverage: the
  // executable uses index*314/3000, floors both trigonometric products, then
  // negates the floored cosine.
  assert((simtower::original_info_clock_point(0, 15) ==
          simtower::OriginalInfoPoint{0, -15}));
  assert((simtower::original_info_clock_point(15, 15) ==
          simtower::OriginalInfoPoint{14, 0}));
  assert((simtower::original_info_clock_point(30, 15) ==
          simtower::OriginalInfoPoint{0, 15}));
  assert((simtower::original_info_clock_point(45, 15) ==
          simtower::OriginalInfoPoint{-15, 1}));
  assert((simtower::original_info_clock_point(15, 9) ==
          simtower::OriginalInfoPoint{8, 0}));

  // 1208:05e6 applies 1000:39b5's signed 32-bit shift before every timestamp.
  assert(simtower::original_coarse_tick(0U) == 0U);
  assert(simtower::original_coarse_tick(15U) == 0U);
  assert(simtower::original_coarse_tick(16U) == 1U);
  assert(simtower::original_coarse_tick(0x7fffffffU) == 0x07ffffffU);
  assert(simtower::original_coarse_tick(0x80000000U) == 0xf8000000U);
  assert(simtower::original_coarse_tick(0xffffffffU) == 0xffffffffU);
  assert(simtower::original_tick_magnitude_delta(900U, 1000U) == 100U);
  assert(simtower::original_tick_magnitude_delta(0x10U, 0xfffffff0U) ==
         0x20U);

  // 1118:0933/09be/0a49/0ad5 share DS:784c but retain their original
  // resource lists, priorities, zero-index clearing, and replacement gates.
  simtower::OriginalInfoStatusState status{};
  assert(simtower::set_original_info_income_status(resources, status, 1U,
                                                    1000U));
  assert(status.text == "Income from Office" && status.priority == 0 &&
         status.started_tick == 1000U);
  assert(simtower::set_original_info_command_status(resources, status, 8U,
                                                     1010U));
  assert(status.text == "Office - $40000" && status.priority == 1 &&
         status.started_tick == 1010U);
  assert(!simtower::set_original_info_income_status(resources, status, 2U,
                                                     1020U));
  assert(status.text == "Office - $40000" && status.started_tick == 1010U);
  assert(simtower::set_original_info_notification_status(resources, status,
                                                          5U, 1030U));
  assert(status.text == "Office workers demand Parking" &&
         status.priority == 2 && status.started_tick == 1030U);
  assert(!simtower::set_original_info_command_status(resources, status, 4U,
                                                      1040U));
  assert(status.text == "Office workers demand Parking");
  assert(simtower::set_original_info_construction_status(resources, status,
                                                          7U, 1050U));
  assert(status.text == "Not enough money for construction" &&
         status.priority == 2 && status.started_tick == 1050U);
  // Direct 1118:08f3 boundary: equality at 300 retains the message and the
  // first greater magnitude delta clears through the construction writer.
  assert(!simtower::expire_original_info_status(status, 1350U));
  assert(simtower::expire_original_info_status(status, 1351U));
  assert(status.text.empty() && status.started_tick == 0U &&
         status.priority == 0);

  // 1000:39ea takes the signed magnitude of a wrapping tick difference.
  assert(simtower::set_original_info_income_status(resources, status, 2U,
                                                    1000U));
  assert(!simtower::expire_original_info_status(status, 900U));
  assert(status.text == "Income from Hotel");
  assert(simtower::expire_original_info_status(status, 699U));

  assert(simtower::set_original_info_command_status(resources, status, 4U,
                                                     2000U));
  assert(simtower::set_original_info_command_status(resources, status, 0U,
                                                     2010U));
  assert(status.text.empty() && status.priority == 0 &&
         status.started_tick == 0U);
  assert(!simtower::expire_original_info_status(status, 0xffffffffU));

  // The 300-unit comparisons above are coarse ticks, not milliseconds:
  // 4,800 ms remains visible and the following 16-ms bucket expires it.
  assert(simtower::set_original_info_income_status(
      resources, status, 1U, simtower::original_coarse_tick(16U)));
  assert(!simtower::expire_original_info_status(
      status, simtower::original_coarse_tick(4816U)));
  assert(simtower::expire_original_info_status(
      status, simtower::original_coarse_tick(4832U)));

  auto tower = simtower::make_original_new_tdt();
  tower.header.rating = 3U;
  tower.header.balance = 123;
  tower.header.frame_time = 0U;
  tower.header.current_day = 0;
  tower.post_elevator.finance.total_population = 456;
  // Direct 1208:0004 coverage: the original shared _wsprintf thunk formats
  // the signed balance/population/date integers used by these Info fields.
  auto content = simtower::build_original_info_content(
      resources, &tower, "Construction message");
  assert(content.rating == 3U);
  assert(content.balance == "$12300");
  assert(content.population == "456");
  assert(content.date_red.empty());
  assert(content.date_dark == "1st WD/1Q/1st Year");
  assert(content.status == "Construction message");
  assert((content.clock == simtower::OriginalInfoClockTime{7, 0}));
  assert((content.minute_hand == simtower::OriginalInfoPoint{0, -15}));

  tower.header.current_day = 1;
  content = simtower::build_original_info_content(resources, &tower);
  assert(content.date_red.empty());
  assert(content.date_dark == "2nd WD/1Q/1st Year");

  tower.header.current_day = 2;
  content = simtower::build_original_info_content(resources, &tower);
  assert(content.date_red == "WE");
  assert(content.date_dark == "/1Q/1st Year");

  tower.header.current_day = 3;
  content = simtower::build_original_info_content(resources, &tower);
  assert(content.date_red.empty());
  assert(content.date_dark == "1st WD/2Q/1st Year");

  tower.header.current_day = 12;
  content = simtower::build_original_info_content(resources, &tower);
  assert(content.date_dark == "1st WD/1Q/2nd Year");

  tower.header.balance = -1;
  content = simtower::build_original_info_content(resources, &tower);
  assert(content.balance == "$-100");

  // 1060:0479 copies the three persisted ten-row finance series and derives
  // the nine DTMP/500 summary fields with signed IDIV and wrapping SUB.
  tower.header.current_day = 14;
  tower.header.other_income = -200;
  tower.header.construction_costs = -300;
  tower.header.last_quarter_money = 400;
  tower.header.balance = 500;
  for (std::size_t index = 0U; index < 10U; ++index) {
    tower.post_elevator.finance.population_by_category[index] =
        static_cast<std::int32_t>(index + 1U);
    tower.post_elevator.finance.income_by_category[index] =
        static_cast<std::int32_t>(100U + index);
    tower.post_elevator.finance.maintenance_by_category[index] =
        -static_cast<std::int32_t>(200U + index);
  }
  tower.post_elevator.finance.total_income = 1000;
  tower.post_elevator.finance.total_maintenance = 250;
  auto finance = simtower::derive_original_finance_view(tower);
  assert(finance.population ==
         tower.post_elevator.finance.population_by_category);
  assert(finance.income == tower.post_elevator.finance.income_by_category);
  assert(finance.maintenance ==
         tower.post_elevator.finance.maintenance_by_category);
  assert(finance.total_income == 1000);
  assert(finance.total_maintenance == 250);
  assert(finance.year == 2);
  assert(finance.quarter == 1);
  assert(finance.net_revenues == 750);
  assert(finance.other_income == -200);
  assert(finance.construction_costs == -300);
  assert(finance.last_quarter_balance == 400);
  assert(finance.total_balance == 500);

  tower.header.current_day = -1;
  tower.post_elevator.finance.total_income =
      std::numeric_limits<std::int32_t>::min();
  tower.post_elevator.finance.total_maintenance = 1;
  finance = simtower::derive_original_finance_view(tower);
  assert(finance.year == 1);
  assert(finance.quarter == 1);
  assert(finance.net_revenues ==
         std::numeric_limits<std::int32_t>::max());
  return 0;
}
