
#include "bainangua.hpp"

#include <catch2/catch_test_macros.hpp>

#include <boost/hana/map.hpp>
#include <boost/hana/string.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/at_key.hpp>

#include "include/RowType.hpp"

using namespace RowType;

namespace {

	TEST_CASE("Basic RowType Tests", "[Basic][RowType]")
	{
		auto simpleRow = boost::hana::make_map(
			boost::hana::make_pair(BOOST_HANA_STRING("name"), std::string("argh")),
			boost::hana::make_pair(boost::hana::int_c<4>, 3.0f)
		);

		REQUIRE(getRowField<"name"_field>(simpleRow) == std::string("argh"));
		REQUIRE(boost::hana::at_key(simpleRow, boost::hana::int_c<4>) == 3.0f);
	}

	TEST_CASE("RowType Wrappers", "[Basic][RowType]")
	{
		auto singleRow = boost::hana::make_map(
			boost::hana::make_pair(BOOST_HANA_STRING("a"), std::string("blargh"))
		);
		auto doubleRow = boost::hana::make_map(
			boost::hana::make_pair(BOOST_HANA_STRING("a"), std::string("blargh")),
			boost::hana::make_pair(boost::hana::int_c<4>, 3.0f)
		);

		auto rowFn =
			AddOneRowWrapper()
			| IdRowWrapper()
			| AddFieldWrapper()
			| PullFromMapFunction();

		REQUIRE(rowFn.applyRow(singleRow) == 9);
		REQUIRE(rowFn.applyRow(doubleRow) == 4);

		auto onlyStringFn =
			OnlyReturnStringWrapper()
			| AddOneRowWrapper()
			| IdRowWrapper()
			| AddFieldWrapper()
			| PullFromMapFunction();

		REQUIRE(onlyStringFn.applyRow(singleRow) == std::string("argh"));
		REQUIRE(onlyStringFn.applyRow(doubleRow) == std::string("argh"));

	}
}