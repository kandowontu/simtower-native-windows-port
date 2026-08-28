#include "original_finance.hpp"

#include <bit>

namespace simtower {
namespace {

std::int32_t wrapping_subtract(std::int32_t lhs,
                               std::int32_t rhs) noexcept {
  return std::bit_cast<std::int32_t>(
      std::bit_cast<std::uint32_t>(lhs) -
      std::bit_cast<std::uint32_t>(rhs));
}

}  // namespace

OriginalFinanceView derive_original_finance_view(
    const OriginalTdtDocument& document) noexcept {
  const auto& finance = document.post_elevator.finance;
  OriginalFinanceView result{};
  result.population = finance.population_by_category;
  result.income = finance.income_by_category;
  result.maintenance = finance.maintenance_by_category;
  result.total_income = finance.total_income;
  result.total_maintenance = finance.total_maintenance;
  result.year = document.header.current_day / 12 + 1;
  result.quarter = (document.header.current_day / 3) % 4 + 1;
  result.net_revenues =
      wrapping_subtract(finance.total_income, finance.total_maintenance);
  result.other_income = document.header.other_income;
  result.construction_costs = document.header.construction_costs;
  result.last_quarter_balance = document.header.last_quarter_money;
  result.total_balance = document.header.balance;
  return result;
}

}  // namespace simtower
