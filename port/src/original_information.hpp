#pragma once

#include "original_dtmp.hpp"
#include "original_resources.hpp"
#include "original_tables.hpp"
#include "original_tdt.hpp"
#include "original_world.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace simtower {

enum class OriginalInformationLauncherKind : std::uint8_t {
  person_rename,
  tenant_rename,
  movie_choice,
};

struct OriginalInformationLauncherContract {
  std::uint16_t dialog_resource_id{};
  bool main_window_owner{};
  bool caller_supplied_owner{};
  bool preserves_dialog_result{};

  friend bool operator==(const OriginalInformationLauncherContract&,
                         const OriginalInformationLauncherContract&) = default;
};

// Exact modal wrapper constants at 1100:39df/3d5b/40d5. Both rename dialogs
// are owned by DS:3258 (Main), even when launched from an information dialog.
// Movie Choice alone uses the caller-supplied owner and preserves DialogBox's
// result for its caller.
[[nodiscard]] constexpr OriginalInformationLauncherContract
original_information_launcher_contract(
    OriginalInformationLauncherKind kind) noexcept {
  switch (kind) {
    case OriginalInformationLauncherKind::person_rename:
      return {730U, true, false, false};
    case OriginalInformationLauncherKind::tenant_rename:
      return {732U, true, false, false};
    case OriginalInformationLauncherKind::movie_choice:
      return {731U, false, true, true};
  }
  return {};
}

enum class OriginalFacilityControlBackground : std::uint8_t {
  null_brush,
  gray_cc,
};

// Exact PEPLEINFODLOGFILTER 1100:020b-027b, 1100:09ad-0a1d,
// ELVINFODLOGFILTER 1100:0fde-104e, and ESCINFODLOGFILTER 1100:1316-1386
// Win16 WM_CTLCOLOR split:
// CTLCOLOR_STATIC (6) uses the palette-matched 0xcccccc brush; every other
// control type is hollow.
[[nodiscard]] constexpr OriginalFacilityControlBackground
original_facility_control_background(bool static_control) noexcept {
  return static_control ? OriginalFacilityControlBackground::gray_cc
                        : OriginalFacilityControlBackground::null_brush;
}

struct OriginalMovieChoiceDialogStyle {
  int font_pixels{};
  bool clear_class_cursor{};
  bool realize_logical_palette{};
  std::uint8_t static_red{};
  std::uint8_t static_green{};
  std::uint8_t static_blue{};

  friend bool operator==(const OriginalMovieChoiceDialogStyle&,
                         const OriginalMovieChoiceDialogStyle&) = default;
};

// Exact MOVIETITLEDIALOGFILTER 1100:4189-423b/4243-4317 presentation
// contract. Both immediate DTMP rendering and WM_PAINT use the palette and
// 13-pixel Arial font; the dialog clears its class cursor, and only static
// controls receive the palette-matched RGB(204,204,204) brush.
[[nodiscard]] constexpr OriginalMovieChoiceDialogStyle
original_movie_choice_dialog_style() noexcept {
  return {13, true, true, 0xccU, 0xccU, 0xccU};
}

enum class OriginalMovieChoiceDialogCommandAction : std::uint8_t {
  none,
  new_release,
  cancel,
  classic,
};

struct OriginalMovieChoiceDialogCommandPlan {
  OriginalMovieChoiceDialogCommandAction action{
      OriginalMovieChoiceDialogCommandAction::none};
  bool consume{};

  friend bool operator==(const OriginalMovieChoiceDialogCommandPlan&,
                         const OriginalMovieChoiceDialogCommandPlan&) =
      default;
};

// MOVIETITLEDIALOGFILTER 1100:431a-43c7 switches on the 16-bit Win16 control
// ID alone. IDs 1/2/3 select New/Cancel/Classic and are consumed; the
// notification word is irrelevant. Every other command falls through FALSE.
[[nodiscard]] constexpr OriginalMovieChoiceDialogCommandPlan
original_movie_choice_dialog_command_plan(std::uint16_t control) noexcept {
  using Action = OriginalMovieChoiceDialogCommandAction;
  if (control == 1U) return {Action::new_release, true};
  if (control == 2U) return {Action::cancel, true};
  if (control == 3U) return {Action::classic, true};
  return {Action::none, false};
}

struct OriginalInformationActivationPlan {
  bool consume{};
  bool activate_nested_modal{};

  friend bool operator==(const OriginalInformationActivationPlan&,
                         const OriginalInformationActivationPlan&) = default;
};

// Exact PEPLEINFODLOGFILTER 1100:01ed-0208, TENANTINFODLOGFILTER
// 1100:0a22-0a3d, ELVINFODLOGFILTER 1100:1053-106e, and
// ESCINFODLOGFILTER 1100:138b-13a6 WM_ACTIVATE branch. Each filter consumes
// the message; a nonzero activation redirects only when DS:31a4 names a
// different nested modal window.
[[nodiscard]] constexpr OriginalInformationActivationPlan
original_information_activation_plan(bool active,
                                     bool nested_modal_present,
                                     bool nested_modal_is_self) noexcept {
  return {true,
          active && nested_modal_present && !nested_modal_is_self};
}

struct OriginalPaintedDialogInitializationFocusPlan {
  bool set_explicit_focus{};
  bool consume{};

  friend bool operator==(const OriginalPaintedDialogInitializationFocusPlan&,
                         const OriginalPaintedDialogInitializationFocusPlan&) =
      default;
};

// AHOTTADLOGFILTER 1068:00d0-02c1, PEPLEINFODLOGFILTER 1100:0145-01ea,
// TENANTINFODLOGFILTER 1100:088a-09aa, ELVINFODLOGFILTER 1100:0f3f-0fdb,
// ESCINFODLOGFILTER 1100:1277-1313, and MOVIETITLEDIALOGFILTER
// 1100:4167-4240 all finish WM_INITDIALOG without a SetFocus call and return
// TRUE. The dialog manager, rather than the filter, therefore chooses the
// initial control from the resource template.
[[nodiscard]] constexpr OriginalPaintedDialogInitializationFocusPlan
original_painted_dialog_initialization_focus_plan() noexcept {
  return {false, true};
}

struct OriginalPersonInformationCapturePlan {
  bool capture_on_initialization{};
  bool release_before_close{};

  friend bool operator==(const OriginalPersonInformationCapturePlan&,
                         const OriginalPersonInformationCapturePlan&) =
      default;
};

// PEPLEINFODLOGFILTER 1100:0145 captures the Person Information window before
// publishing its modal state. Its ID-1 close path at 1100:030d-032b releases
// that capture after DTMP cleanup and before EndDialog.
[[nodiscard]] constexpr OriginalPersonInformationCapturePlan
original_person_information_capture_plan() noexcept {
  return {true, true};
}

enum class OriginalMagnifierTargetKind : std::uint8_t {
  none,
  elevator_car_information,
  elevator_control,
  vertical_transport_information,
  waiting_person_information,
  facility_information,
};

struct OriginalMagnifierTarget {
  static constexpr std::size_t kNoIndex = static_cast<std::size_t>(-1);

  OriginalMagnifierTargetKind kind{OriginalMagnifierTargetKind::none};
  std::uint16_t dialog_id{};
  std::size_t elevator_index{kNoIndex};
  std::int16_t elevator_car_index{-1};
  std::size_t vertical_transport_index{kNoIndex};
  std::size_t person_index{kNoIndex};
  std::int16_t floor{-1};
  std::size_t tenant_index{kNoIndex};

  [[nodiscard]] constexpr bool handled() const noexcept {
    return kind != OriginalMagnifierTargetKind::none;
  }

  friend bool operator==(const OriginalMagnifierTarget&,
                         const OriginalMagnifierTarget&) = default;
};

struct OriginalTransportInformationClickPlan {
  bool select_palette{};
  bool realize_palette{};
  bool restore_topmost{};
  bool restore_modal_target{};
  bool consume{};

  friend bool operator==(const OriginalTransportInformationClickPlan&,
                         const OriginalTransportInformationClickPlan&) =
      default;
};

// Exact outer WM_LBUTTONDOWN contracts from ELVINFODLOGFILTER
// 1100:113a-11a8 and ESCINFODLOGFILTER 1100:13fc-14e9. Both filters acquire a
// DC, realize the palette, run the portrait hit/drill-down helper, restore the
// information window as TOPMOST and DS:31a4, and consume even an empty-panel
// click. Only the Stair/Escalator filter explicitly selects the logical
// palette before realizing it.
[[nodiscard]] constexpr OriginalTransportInformationClickPlan
original_transport_information_click_plan(
    OriginalMagnifierTargetKind kind) noexcept {
  if (kind == OriginalMagnifierTargetKind::elevator_car_information) {
    return {false, true, true, true, true};
  }
  if (kind == OriginalMagnifierTargetKind::vertical_transport_information) {
    return {true, true, true, true, true};
  }
  return {};
}

struct OriginalFacilityInformationClickPlan {
  bool select_palette{};
  bool realize_palette{};
  bool restore_modal_target{};
  bool restore_topmost{};
  bool consume{};

  friend bool operator==(const OriginalFacilityInformationClickPlan&,
                         const OriginalFacilityInformationClickPlan&) =
      default;
};

// TENANTINFODLOGFILTER 1100:0d5a-0e54 always selects and realizes the logical
// palette, dispatches the group-specific portrait helper, restores DS:31a4 and
// TOPMOST, and returns TRUE. These outer actions are unconditional even when
// the click misses every live-person rectangle.
[[nodiscard]] constexpr OriginalFacilityInformationClickPlan
original_facility_information_click_plan() noexcept {
  return {true, true, true, true, true};
}

enum class OriginalFacilityInformationCommandAction : std::uint8_t {
  none,
  close,
  rename,
  change_rent,
  choose_movie,
};

struct OriginalFacilityInformationCommandPlan {
  OriginalFacilityInformationCommandAction action{
      OriginalFacilityInformationCommandAction::none};
  bool consume{};

  friend bool operator==(const OriginalFacilityInformationCommandPlan&,
                         const OriginalFacilityInformationCommandPlan&) =
      default;
};

// TENANTINFODLOGFILTER 1100:0b11-0d57 consumes every WM_COMMAND, including
// unknown IDs and irrelevant notifications. IDs 1 and 7 ignore the Win16
// notification word. ID 13 acts only for notification zero or one: dialog
// groups 0..5 change rent, and group 10 opens New Movie.
[[nodiscard]] constexpr OriginalFacilityInformationCommandPlan
original_facility_information_command_plan(
    std::uint16_t control,
    std::uint16_t notification,
    std::uint8_t dialog_group) noexcept {
  using Action = OriginalFacilityInformationCommandAction;
  if (control == 1U) return {Action::close, true};
  if (control == 7U) return {Action::rename, true};
  if (control == 13U && notification <= 1U) {
    if (dialog_group <= 5U) return {Action::change_rent, true};
    if (dialog_group == 10U) return {Action::choose_movie, true};
  }
  return {Action::none, true};
}

struct OriginalFacilityPersonSprite {
  std::size_t person_index{OriginalMagnifierTarget::kNoIndex};
  std::uint16_t bitmap_id{700U};
  std::int16_t frame{-1};
  int destination_x{};
  int destination_y{};
  int width{};
  int height{24};

  friend bool operator==(const OriginalFacilityPersonSprite&,
                         const OriginalFacilityPersonSprite&) = default;
};

// Exact 1100:35b7 PTINRECT behavior shared by information portrait rows.
// Right and bottom edges are excluded, as in the Win16 API.
[[nodiscard]] std::optional<std::size_t>
original_information_person_sprite_hit(
    std::span<const OriginalFacilityPersonSprite> sprites,
    int x,
    int y) noexcept;

// Exact information cursor-zone test. 1100:4fba tests DTMP item 4 alone;
// 1100:5043 also tests item 9 for Elevator/Stair information and Tenant
// dialog groups 9..11. Matching Win16 PTINRECT excludes right/bottom edges.
[[nodiscard]] bool original_information_portrait_panel_hit(
    const OriginalDtmp& dtmp,
    int x,
    int y,
    bool include_item_9 = true) noexcept;

struct OriginalTransportInformationText {
  bool valid{};
  std::string primary{};
  std::string secondary{};
  std::vector<OriginalFacilityPersonSprite> person_sprites{};
};

enum class OriginalPersonPortraitVariant : std::uint8_t {
  normal,
  named,
  vip,
};

// Exact 1100:43ed rename-dialog gate: edit item four enables the OK button
// for any nonempty text and disables it only at length zero.
[[nodiscard]] constexpr bool original_rename_ok_enabled(
    std::size_t text_length) noexcept {
  return text_length != 0U;
}

struct OriginalInformationMeter {
  bool visible{};
  std::int16_t value{};
  std::int16_t lower{};
  std::int16_t upper{};
  std::int16_t maximum{300};
  std::uint8_t band{};

  friend bool operator==(const OriginalInformationMeter&,
                         const OriginalInformationMeter&) = default;
};

// Exact 1140:019d rating selector shared by Person and Facility Information.
// Ratings 1/2, 3, and 4+ select the three signed lower/upper PART pairs.
[[nodiscard]] std::pair<std::int16_t, std::int16_t>
original_information_thresholds(
    const OriginalTdtDocument& document,
    const OriginalPartTable& part) noexcept;

// Exact signed endpoint geometry shared by reverse meter 11e0:01d8 and
// forward meter 11e0:0358 after their rectangles are deflated by two pixels.
// Integer division truncates toward zero, matching the recovered IDIV.
[[nodiscard]] constexpr int original_information_meter_fill_right(
    int left,
    int right,
    std::int16_t value,
    std::int16_t maximum,
    bool reverse) noexcept {
  if (reverse) {
    if (value >= maximum) return left;
    if (value <= 0) return right;
    return left + static_cast<int>(
        static_cast<std::int64_t>(right - left) *
        static_cast<std::int64_t>(maximum - value) / maximum);
  }
  if (value < 0) return left;
  if (value >= maximum) return right;
  return left + static_cast<int>(
      static_cast<std::int64_t>(right - left) * value / maximum);
}

// Literal Win16 COLORREF inputs selected by 11e0:025f/026b/0277 and
// 11e0:03d2/03de/03ea before logical-palette resolution.
[[nodiscard]] constexpr std::uint32_t original_information_meter_colorref(
    std::uint8_t band) noexcept {
  return band == 0U ? 0x00ff0000U
       : band == 1U ? 0x0000ffffU
                    : 0x000000ffU;
}

struct OriginalPersonInformation {
  bool valid{};
  std::size_t person_index{OriginalMagnifierTarget::kNoIndex};
  std::uint16_t dialog_id{};
  std::int16_t owner_floor{-1};
  std::uint8_t owner_key{};
  std::size_t owner_tenant_index{OriginalMagnifierTarget::kNoIndex};
  std::int8_t owner_type{-1};
  std::int16_t portrait_frame{-1};
  OriginalPersonPortraitVariant portrait_variant{
      OriginalPersonPortraitVariant::normal};
  std::string display_name{};
  std::string origin_text{};
  std::string activity_text{};
  OriginalInformationMeter evaluation{};
  OriginalInformationMeter stress{};
};

enum class OriginalPersonNameStatus : std::uint8_t {
  invalid_person,
  empty,
  too_long,
  full,
  added,
  updated,
  removed,
  not_named,
};

struct OriginalPersonNameResult {
  OriginalPersonNameStatus status{OriginalPersonNameStatus::invalid_person};
  bool changed{};
};

struct OriginalFacilityPreview {
  int view_x{};
  int view_y{};
  int width{};
  int height{};

  [[nodiscard]] constexpr bool valid() const noexcept {
    return width > 0 && height > 0;
  }

  friend bool operator==(const OriginalFacilityPreview&,
                         const OriginalFacilityPreview&) = default;
};

struct OriginalFacilityPreviewBackingCounts {
  std::int16_t visible_cells{};   // DS:777e
  std::int16_t visible_floors{};  // DS:7780
  std::int16_t cache_columns{};   // DS:7782 = visible_cells * 2
  std::int16_t cache_rows{};      // DS:7784 = visible_floors

  friend bool operator==(const OriginalFacilityPreviewBackingCounts&,
                         const OriginalFacilityPreviewBackingCounts&) =
      default;
};

// Exact temporary-grid expansion at 1100:4514. This changes the backing used
// for the pre-dialog world snapshot; it does not clamp 1100:4869's crop RECT.
[[nodiscard]] OriginalFacilityPreviewBackingCounts
original_facility_preview_backing_counts(
    const OriginalTdtDocument& document,
    std::int16_t floor_number,
    std::size_t tenant_index,
    std::int16_t current_visible_cells,
    std::int16_t current_visible_floors) noexcept;

struct OriginalFacilityPreviewDestination {
  int left{};
  int top{};
  int right{};
  int bottom{};

  [[nodiscard]] constexpr int width() const noexcept { return right - left; }
  [[nodiscard]] constexpr int height() const noexcept { return bottom - top; }

  friend bool operator==(const OriginalFacilityPreviewDestination&,
                         const OriginalFacilityPreviewDestination&) = default;
};

// Exact signed-integer destination-rectangle scaler at 1100:4d1d. Preview
// enlargement is capped at 200%; when both container dimensions are smaller,
// the original deliberately uses cover/crop behavior rather than contain.
[[nodiscard]] OriginalFacilityPreviewDestination
original_facility_preview_destination(
    const OriginalFacilityPreview& preview,
    int container_left,
    int container_top,
    int container_right,
    int container_bottom) noexcept;

// Exact native presentation boundary for 1100:4439. The original fills DTMP
// item 2 with RGB(0xcc,0xcc,0xcc), applies 1100:4d1d's destination geometry,
// and WinGStretchBlts the retained pre-dialog world snapshot into that area.
void draw_original_facility_preview(
    HDC destination,
    const OriginalWorldRaster& raster,
    const OriginalFacilityPreview& preview,
    const RECT& container) noexcept;

struct OriginalFacilityAdvisoryTextOffset {
  int x{};
  int y{};

  friend bool operator==(const OriginalFacilityAdvisoryTextOffset&,
                         const OriginalFacilityAdvisoryTextOffset&) = default;
};

// 1100:1760-176c gets DTMP item 8 through 11e0:0049, whose shared point
// helper returns (left+6, top+15), then adds (2,3). 1108:08e4 advances the
// shared advisory-line counter by sixteen pixels for each subsequent line.
[[nodiscard]] constexpr OriginalFacilityAdvisoryTextOffset
original_facility_advisory_text_offset(std::size_t line) noexcept {
  return {8, 18 + static_cast<int>(line) * 16};
}

struct OriginalFacilityInformation {
  bool valid{};
  std::uint16_t dialog_id{};
  std::uint8_t dialog_group{};
  std::int16_t floor{-1};
  std::size_t tenant_index{OriginalMagnifierTarget::kNoIndex};
  std::int8_t type{-1};
  std::size_t linked_record_index{OriginalMagnifierTarget::kNoIndex};
  std::string display_name{};
  std::string occupancy_text{};
  std::string age_text{};
  std::string movie_title{};
  std::string movie_length_text{};
  std::string movie_income_text{};
  std::string commercial_value_text{};
  std::string yesterday_profit_text{};
  OriginalInformationMeter evaluation{};
  OriginalInformationMeter commercial_meter{};
  std::array<std::string, 4> rent_choices{};
  std::uint8_t selected_rent_rate{};
  bool rent_control_visible{};
  bool rent_control_enabled{};
  OriginalFacilityPreview preview{};
  std::array<std::string, 3> advisory_lines{};
  std::uint8_t advisory_line_count{};
  std::vector<OriginalFacilityPersonSprite> person_sprites{};
};

enum class OriginalTenantNameStatus : std::uint8_t {
  invalid_tenant,
  empty,
  too_long,
  full,
  added,
  updated,
  removed,
  not_named,
};

struct OriginalTenantNameResult {
  OriginalTenantNameStatus status{OriginalTenantNameStatus::invalid_tenant};
  bool changed{};
};

enum class OriginalMovieChoice : std::uint8_t {
  new_release = 1,
  cancel = 2,
  classic = 3,
};

struct OriginalMovieChoiceResult {
  bool handled{};
  bool changed{};
  bool affordable{};
  std::int32_t cost{};
};

// Exact WM_LBUTTONDOWN precedence at 1058:015b-019d: Elevator car/control,
// Stair/Escalator, a person in either Elevator waiting lane, then a floor
// facility. The returned numeric resource is selected by the original
// 1098/1100 routines; no dialog is synthesized here.
[[nodiscard]] OriginalMagnifierTarget select_original_magnifier_target(
    const OriginalTdtDocument& document,
    int client_x,
    int client_y,
    int view_x,
    int view_y) noexcept;

// Exact two text fields and live passenger portraits painted by
// 1100:1b53/1cbb/327f/3431/35b7 into DIALOG 761/762.
// Elevator text is STRL/400[type+1] and "passengers / capacity"; vertical
// transport text is STRL/400[(shape&1)+4] and word_6+word_8 with signed
// 16-bit wrapping. Portraits retain the item-4/item-9 DTMP rows and
// BITMAP/700/702/703 normal/named/VIP selection.
[[nodiscard]] OriginalTransportInformationText
original_transport_information_text(
    const OriginalResources& resources,
    const OriginalTdtDocument& document,
    const OriginalMagnifierTarget& target);

// Exact 1188:04db/0541/05a7 fixed-table lookup used by the Person
// Information and Rename Person procedures.
[[nodiscard]] std::optional<std::size_t> original_person_name_slot(
    const OriginalTdtDocument& document,
    std::size_t person_index) noexcept;
[[nodiscard]] std::string original_person_saved_name(
    const OriginalTdtDocument& document,
    std::size_t person_index);

// Exact add/update and removal transactions at 1188:061c/0793. The input is
// the original ANSI byte string: names are nonempty, at most fifteen bytes,
// and the persisted record is exactly sixteen bytes including the NUL.
[[nodiscard]] OriginalPersonNameResult set_original_person_name(
    OriginalTdtDocument& document,
    std::size_t person_index,
    std::string_view name) noexcept;
[[nodiscard]] OriginalPersonNameResult remove_original_person_name(
    OriginalTdtDocument& document,
    std::size_t person_index) noexcept;

enum class OriginalPersonInformationContext : std::uint8_t {
  main_world = 0,
  transport_dialog = 1,
  facility_dialog = 2,
};

// Static translation of 1100:151b and its 1100:21a1, 1100:2236,
// 1100:232e, 1100:1f2e, and 1100:1dca helpers. It produces every
// person-specific field painted into DTMP/763 or DTMP/764 without running the
// original executable. The context preserves DS:b3a6: main-world waiting
// people use their live elapsed wait, transport passengers use the retained
// low-ten-bit metric, and facility occupants use zero.
[[nodiscard]] OriginalPersonInformation original_person_information(
    const OriginalResources& resources,
    const OriginalTdtDocument& document,
    const OriginalPartTable& part,
    std::size_t person_index,
    OriginalPersonInformationContext context =
        OriginalPersonInformationContext::main_world);

// Exact dd34/tenant-name transactions at 1188:050f/0575/06dc/0884 and
// 0aa0. Tenant names share the source executable's twenty fixed link slots
// and sixteen-byte ANSI name records.
[[nodiscard]] std::optional<std::size_t> original_tenant_name_slot(
    const OriginalTdtDocument& document,
    std::int16_t floor,
    std::size_t tenant_index) noexcept;
[[nodiscard]] std::string original_tenant_saved_name(
    const OriginalTdtDocument& document,
    std::int16_t floor,
    std::size_t tenant_index);
[[nodiscard]] OriginalTenantNameResult set_original_tenant_name(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::size_t tenant_index,
    std::string_view name) noexcept;
[[nodiscard]] OriginalTenantNameResult remove_original_tenant_name(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::size_t tenant_index) noexcept;

// Complete data model painted by 1100:1716 into DTMP/748..760. The preview
// rectangle is a crop of the translated logical-world renderer, not recreated
// artwork; all labels, status codes, metrics and movie strings come from the
// original STRL/PART/TDT records.
// Exact 1100:0644 combo-box literals. Groups 0..5 each contain four rent
// choices; every other group leaves the reset combo empty.
[[nodiscard]] std::array<std::string, 4> original_rent_choices(
    std::uint8_t group);

// Exact linked-service state and open-hours core of 1108:05e3. The caller
// owns its group-12 and three-line-cap gates; this predicate preserves CBW
// service state plus the strict signed Restaurant/Fast Food clock intervals.
[[nodiscard]] bool original_commercial_closed_advisory_required(
    std::int8_t facility_type,
    std::int8_t linked_status,
    std::uint16_t frame_time) noexcept;

// Exact 1100:25d9 Movie Information program-length text. Record byte nine is
// CBW-sign-extended, divided by three with truncation toward zero, and switches
// to STRL/713 item six at signed age twelve.
[[nodiscard]] std::string original_movie_length_text(
    const OriginalResources& resources,
    const std::array<std::byte, 0x0c>& record);

// Exact 1100:268e -> 1180:0bcb Movie Information income lookup. Attendance
// byte eleven and all three PART thresholds/four return values are signed,
// preserving the executable's CBW and signed-word comparisons.
[[nodiscard]] std::int32_t original_movie_information_income(
    const std::array<std::byte, 0x0c>& record,
    const OriginalPartTable& part) noexcept;

[[nodiscard]] OriginalFacilityInformation original_facility_information(
    const OriginalResources& resources,
    const OriginalTdtDocument& document,
    const OriginalPartTable& part,
    std::int16_t floor,
    std::size_t tenant_index);

// Exact CBN_SELCHANGE tail at 1100:0bb7-0c30. It writes tenant byte 16 and
// immediately invokes the translated 1130:06e9 satisfaction path.
[[nodiscard]] bool set_original_facility_rent_rate(
    OriginalTdtDocument& document,
    const OriginalPartTable& part,
    std::int16_t floor,
    std::size_t tenant_index,
    std::uint8_t rent_rate) noexcept;

// Exact DIALOG/731 transaction at 1100:4138 plus 1180:0de9. Costs are the
// original internal money units (displayed dollars are scaled by 100).
[[nodiscard]] OriginalMovieChoiceResult choose_original_movie(
    OriginalTdtDocument& document,
    const OriginalPartTable& part,
    std::size_t linked_record_index,
    OriginalMovieChoice choice) noexcept;

}  // namespace simtower
