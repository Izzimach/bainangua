#include "RowType.hpp"

#include <benchmark/benchmark.h>
#include <boost/hana/map.hpp>

//
// boost::hana::map benchmarks. We compare accessing ints from a hana map and compare it to simply accessing ints from
// a dumb struct.  There are several benchmarks, each using a different number of fields. (3,4,5)
//

struct ThreeInts {
    int x;
    int y;
    int z;
};

static void BM_StructAccess3(benchmark::State& state) {
    ThreeInts val(1, 2, 3);
    for (auto _ : state)
    {
        int result = val.x + val.y + val.z;
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_StructAccess3);

static void BM_HanaMapAccess3(benchmark::State& state) {
    auto hanamap = boost::hana::make_map(
        boost::hana::make_pair(BOOST_HANA_STRING("x"), 1),
        boost::hana::make_pair(BOOST_HANA_STRING("y"), 2),
        boost::hana::make_pair(BOOST_HANA_STRING("z"), 3)
    );

    for (auto _ : state)
    {
        int result =
            boost::hana::at_key(hanamap, BOOST_HANA_STRING("x")) +
            boost::hana::at_key(hanamap, BOOST_HANA_STRING("y")) +
            boost::hana::at_key(hanamap, BOOST_HANA_STRING("z"));
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_HanaMapAccess3);


struct FourInts {
    int x;
    int y;
    int z;
    int a;
};

static void BM_StructAccess4(benchmark::State& state) {
    FourInts val(1, 2, 3, 4);
    for (auto _ : state)
    {
        int result = val.x + val.y + val.z + val.a;
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_StructAccess4);

static void BM_HanaMapAccess4(benchmark::State& state) {
    auto hanamap = boost::hana::make_map(
        boost::hana::make_pair(BOOST_HANA_STRING("x"), 1),
        boost::hana::make_pair(BOOST_HANA_STRING("y"), 2),
        boost::hana::make_pair(BOOST_HANA_STRING("z"), 3),
        boost::hana::make_pair(BOOST_HANA_STRING("a"), 4)
    );

    for (auto _ : state)
    {
        int result =
            boost::hana::at_key(hanamap, BOOST_HANA_STRING("x")) +
            boost::hana::at_key(hanamap, BOOST_HANA_STRING("y")) +
            boost::hana::at_key(hanamap, BOOST_HANA_STRING("z")) +
            boost::hana::at_key(hanamap, BOOST_HANA_STRING("a"));
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_HanaMapAccess4);


struct FiveInts {
    int x;
    int y;
    int z;
    int a;
    int b;
};

static void BM_StructAccess5(benchmark::State& state) {
    FiveInts val(1, 2, 3, 4, 5);
    for (auto _ : state)
    {
        int result = val.x + val.y + val.z + val.a + val.b;
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_StructAccess5);

static void BM_HanaMapAccess5(benchmark::State& state) {
    auto hanamap = boost::hana::make_map(
        boost::hana::make_pair(BOOST_HANA_STRING("x"), 1),
        boost::hana::make_pair(BOOST_HANA_STRING("y"), 2),
        boost::hana::make_pair(BOOST_HANA_STRING("z"), 3),
        boost::hana::make_pair(BOOST_HANA_STRING("a"), 4),
        boost::hana::make_pair(BOOST_HANA_STRING("b"), 5)
    );

    for (auto _ : state)
    {
        int result =
            boost::hana::at_key(hanamap, BOOST_HANA_STRING("x")) +
            boost::hana::at_key(hanamap, BOOST_HANA_STRING("y")) +
            boost::hana::at_key(hanamap, BOOST_HANA_STRING("z")) +
            boost::hana::at_key(hanamap, BOOST_HANA_STRING("a")) +
            boost::hana::at_key(hanamap, BOOST_HANA_STRING("b"));
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_HanaMapAccess5);
