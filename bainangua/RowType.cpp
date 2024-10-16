
#include "include/RowType.hpp"

#include <iostream>
#include <format>

void RowType::testRowTypes() {
	auto singleRow = boost::hana::make_map(
		boost::hana::make_pair(BOOST_HANA_STRING("a"), std::string("blargh"))
	);
	auto doubleRow = boost::hana::make_map(
		boost::hana::make_pair(BOOST_HANA_STRING("a"), std::string("blargh")),
		boost::hana::make_pair(boost::hana::int_c<4>, 3.0f)
	);

	auto rowFn =
		OnlyReturnStringWrapper()
		| AddOneRowWrapper()
		| IdRowWrapper()
		| AddFieldWrapper()
		| PullFromMapFunction();
	auto val = rowFn.applyRow(singleRow);

	std::cout << std::format("{}\n", val);
}
