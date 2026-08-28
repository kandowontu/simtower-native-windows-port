#include "original_resources.hpp"
#include "original_tables.hpp"
#include "original_ui.hpp"

#include <cassert>

int main() {
  const auto resources = simtower::OriginalResources::from_current_module();
  // Direct 1208:0cb5/1208:0d75 replacement coverage. Win16 unlocked every
  // outstanding GlobalResource lock and freed the handle; the PE RCDATA
  // replacement is immutable process-owned storage, so repeated lookups must
  // retain the same stable bytes without a lock-count lifecycle.
  const auto first_bitmap = resources.find("BITMAP", 128);
  const auto second_bitmap = resources.find("BITMAP", 128);
  assert(first_bitmap.data() == second_bitmap.data());
  assert(first_bitmap.size() == second_bitmap.size());
  assert(resources.pack().size() == 5'758'464);
  assert(resources.find("BITMAP", 128).size() == 48'128);
  assert(resources.find("DTMP", 3000).size() == 512);
  assert(resources.find("PART", 1000).size() == 512);
  assert(resources.find("YEN", 1000).size() == 512);
  assert(resources.find("STRL", 1000).size() == 512);
  assert(!simtower::original_strl_entry(resources.find("STRL", 1000), 1).empty());

  const HRSRC help_resource = FindResourceW(
      GetModuleHandleW(nullptr), MAKEINTRESOURCEW(102), MAKEINTRESOURCEW(10));
  assert(help_resource != nullptr);
  const HGLOBAL help_loaded = LoadResource(
      GetModuleHandleW(nullptr), help_resource);
  const auto* help_bytes = static_cast<const std::uint8_t*>(
      LockResource(help_loaded));
  assert(help_bytes != nullptr);
  assert(SizeofResource(GetModuleHandleW(nullptr), help_resource) == 584831U);
  assert(help_bytes[0] == 0x3fU && help_bytes[1] == 0x5fU &&
         help_bytes[2] == 0x03U && help_bytes[3] == 0x00U);

  const HMENU menu = simtower::create_original_menu(resources.find("MENU", "TOWER_MENU"));
  assert(menu != nullptr);
  assert(GetMenuItemCount(menu) == 4);
  const HMENU file_menu = GetSubMenu(menu, 0);
  assert(file_menu != nullptr);
  assert(GetMenuItemID(file_menu, 0) == 40005U);
  assert(GetMenuItemID(file_menu, 6) == 40001U);
  DestroyMenu(menu);

  const HACCEL accelerators = simtower::create_original_accelerators(
      resources.find("ACCELERATOR", "TOWER_MENU"));
  ACCEL accelerator{};
  assert(CopyAcceleratorTableW(accelerators, &accelerator, 1) == 1);
  assert(accelerator.key == VK_F1);
  assert(accelerator.cmd == 40021U);
  DestroyAcceleratorTable(accelerators);

  const HICON icon = simtower::create_original_icon(resources, "TOWER_APPICON");
  assert(icon != nullptr);
  DestroyIcon(icon);
  return 0;
}
