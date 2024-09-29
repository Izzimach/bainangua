
#include "gtest/gtest.h"
#include "bainangua.hpp"

#include <boost/hana/map.hpp>
#include <boost/hana/string.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/at_key.hpp>

#include "include/RowType.hpp"

using namespace RowType;

namespace {

	TEST(RowType, BasicTest)
	{
		auto simpleRow = boost::hana::make_map(
			boost::hana::make_pair(BOOST_HANA_STRING("name"), std::string("argh")),
			boost::hana::make_pair(boost::hana::int_c<4>, 3.0f)
		);

		EXPECT_EQ(getRowField<"name"_field>(simpleRow), std::string("argh"));
		EXPECT_EQ(boost::hana::at_key(simpleRow, boost::hana::int_c<4>), 3.0f);
	}

	TEST(RowType, Wrappers)
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

		EXPECT_EQ(rowFn.applyRow(singleRow), 9);
		EXPECT_EQ(rowFn.applyRow(doubleRow), 4);

		auto onlyStringFn =
			OnlyReturnStringWrapper()
			| AddOneRowWrapper()
			| IdRowWrapper()
			| AddFieldWrapper()
			| PullFromMapFunction();

		EXPECT_EQ(onlyStringFn.applyRow(singleRow), std::string("argh"));
		EXPECT_EQ(onlyStringFn.applyRow(doubleRow), std::string("argh"));

	}
}