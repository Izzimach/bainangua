
#include "bainangua.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

#include <boost/hana/map.hpp>
#include <boost/hana/string.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/at_key.hpp>

#include "include/RowType.hpp"

using namespace RowType;

namespace RowTypeTests {
	struct ZeroRowFunction {
		using row_tag = RowFunctionTag;
		using return_type = double;

		template<typename Row>
		constexpr double applyRow(Row) { return 0.0; }
	};

	template <typename V>
	struct PullFromMapFunction {
		using row_tag = RowFunctionTag;
		using return_type = V;

		template <typename Row>
		constexpr V applyRow(Row r) {
			return boost::hana::at_key(r, boost::hana::int_c<4>);
		}
	};


	struct IdRowWrapper {
		using row_tag = RowWrapperTag;

		template <typename WrappedReturnType>
		using return_type_transformer = WrappedReturnType;

		template <typename RowFunction, typename Row>
		constexpr RowFunction::return_type wrapRowFunction(RowFunction f, Row r) {
			return f.applyRow(r);
		}
	};

	struct OnlyReturnStringWrapper {
		using row_tag = RowWrapperTag;

		template <typename WrappedReturnType>
		using return_type_transformer = std::string;

		template <typename RowFunction, typename Row>
		constexpr std::string wrapRowFunction(RowFunction f, Row r) {
			f.applyRow(r);
			return std::string("argh");
		}
	};

	struct AddOneRowWrapper {
		using row_tag = RowWrapperTag;

		template <typename WrappedReturnType>
		using return_type_transformer = WrappedReturnType;

		template <typename RowFunction, typename Row>
		constexpr RowFunction::return_type wrapRowFunction(RowFunction f, Row r) { return 1 + f.applyRow(r); }
	};

	struct AddFieldWrapper {
		using row_tag = RowWrapperTag;

		template <typename WrappedReturnType>
		using return_type_transformer = WrappedReturnType;

		template <typename RowFunction, typename Row>
		constexpr RowFunction::return_type wrapRowFunction(RowFunction f, Row r) {
			auto r2 = boost::hana::insert(r, boost::hana::make_pair(boost::hana::int_c<4>, 8.0f));
			return f.applyRow(r2);
		}
	};


	TEST_CASE("Basic RowType Tests", "[Basic][RowType]")
	{
		auto simpleRow = boost::hana::make_map(
			boost::hana::make_pair(BOOST_HANA_STRING("name"), std::string("argh")),
			boost::hana::make_pair(boost::hana::int_c<4>, 3.0f)
		);

		REQUIRE(getRowField<"name"_field>(simpleRow) == std::string("argh"));
		REQUIRE(boost::hana::at_key(simpleRow, boost::hana::int_c<4>) == 3.0f);

// skip benchmarks in debug builds
#ifdef NDEBUG
		//
		// boost::hana::map benchmarks. We compare accessing ints from a hana map and compare it to simply accessing ints from
		// a dumb struct.  There are several benchmarks, each using a different number of fields. (3,4,5)
		//

		struct ThreeInts {
			int x;
			int y;
			int z;
		};

		BENCHMARK_ADVANCED("struct accesss - 3 fields")(Catch::Benchmark::Chronometer meter) {
			ThreeInts val(1, 2, 3);
			meter.measure([&val] { return val.x + val.y + val.z; });
		};

		BENCHMARK_ADVANCED("tuple access - 3 fields")(Catch::Benchmark::Chronometer meter) {
			auto val = std::make_tuple(1, 2, 3);
			meter.measure([&val] { return std::get<0>(val) + std::get<1>(val) + std::get<2>(val); });
		};

		BENCHMARK_ADVANCED("boost::hana::map accesss - 3 fields")(Catch::Benchmark::Chronometer meter) {
			auto hanamap = boost::hana::make_map(
				boost::hana::make_pair(BOOST_HANA_STRING("x"), 1),
				boost::hana::make_pair(BOOST_HANA_STRING("y"), 2),
				boost::hana::make_pair(BOOST_HANA_STRING("z"), 3)
			);

			meter.measure([&hanamap] {
				int result =
					boost::hana::at_key(hanamap, BOOST_HANA_STRING("x")) +
					boost::hana::at_key(hanamap, BOOST_HANA_STRING("y")) +
					boost::hana::at_key(hanamap, BOOST_HANA_STRING("z"));
				return result;
				});
		};
		BENCHMARK_ADVANCED("boost::hana::map accesss - 3 fields - backwards")(Catch::Benchmark::Chronometer meter) {
			auto hanamap = boost::hana::make_map(
				boost::hana::make_pair(BOOST_HANA_STRING("x"), 1),
				boost::hana::make_pair(BOOST_HANA_STRING("y"), 2),
				boost::hana::make_pair(BOOST_HANA_STRING("z"), 3)
			);

			meter.measure([&hanamap] {
				int result =
					boost::hana::at_key(hanamap, BOOST_HANA_STRING("z")) +
					boost::hana::at_key(hanamap, BOOST_HANA_STRING("y")) +
					boost::hana::at_key(hanamap, BOOST_HANA_STRING("x"));
				return result;
				});
		};


		struct FourInts {
			int x;
			int y;
			int z;
			int a;
		};

		BENCHMARK_ADVANCED("struct accesss - 4 fields")(Catch::Benchmark::Chronometer meter) {
			FourInts val(1, 2, 3, 4);
			meter.measure([&val] {return val.x + val.y + val.z + val.a; });
		};

		BENCHMARK_ADVANCED("boost::hana::map accesss - 4 fields")(Catch::Benchmark::Chronometer meter) {
			auto hanamap = boost::hana::make_map(
				boost::hana::make_pair(BOOST_HANA_STRING("x"), 1),
				boost::hana::make_pair(BOOST_HANA_STRING("y"), 2),
				boost::hana::make_pair(BOOST_HANA_STRING("z"), 3),
				boost::hana::make_pair(BOOST_HANA_STRING("a"), 4)
			);

			meter.measure([&hanamap] {
				return
					boost::hana::at_key(hanamap, BOOST_HANA_STRING("x")) +
					boost::hana::at_key(hanamap, BOOST_HANA_STRING("y")) +
					boost::hana::at_key(hanamap, BOOST_HANA_STRING("z")) +
					boost::hana::at_key(hanamap, BOOST_HANA_STRING("a"));
				});
		};
#endif

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
			| PullFromMapFunction<float>();

		REQUIRE(rowFn.applyRow(singleRow) == 9.0f);
		REQUIRE(rowFn.applyRow(doubleRow) == 4.0f);

		auto onlyStringFn =
			OnlyReturnStringWrapper()
			| AddOneRowWrapper()
			| IdRowWrapper()
			| AddFieldWrapper()
			| PullFromMapFunction<float>();

		REQUIRE(onlyStringFn.applyRow(singleRow) == std::string("argh"));
		REQUIRE(onlyStringFn.applyRow(doubleRow) == std::string("argh"));

	}
}