
#include <benchmark/benchmark.h>

#include <immer/array.hpp>
#include <immer/vector.hpp>
#include <array>

static void BM_ImmerArrayForEach(benchmark::State& state) {
    std::array<int, 20> dataSource{ 1,9,4,2,13, 14,9,1,3,2, 1,9,4,2,13, 14,9,1,3,2 };
    for (auto _ : state)
    {
        immer::array<int> processArray;
        std::for_each(dataSource.begin(), dataSource.end(), [&](int v) { processArray = processArray.push_back(v); });
        benchmark::DoNotOptimize(processArray);
    }
}
BENCHMARK(BM_ImmerArrayForEach);

static void BM_ImmerArrayAccumulate(benchmark::State& state) {
    std::array<int, 20> dataSource{ 1,9,4,2,13, 14,9,1,3,2, 1,9,4,2,13, 14,9,1,3,2 };
    for (auto _ : state)
    {
        immer::array<int> processArray =
            std::accumulate(
                dataSource.begin(),
                dataSource.end(),
                immer::array<int>(),
                [&](auto vals, int val) { return vals.push_back(val); }
                );
        benchmark::DoNotOptimize(processArray);
    }
}
BENCHMARK(BM_ImmerArrayAccumulate);


static void BM_ImmerVectorForEach(benchmark::State& state) {
    std::array<int, 10> dataSource{ 1,9,4,2,13, 14,9,1,3,2 };
    for (auto _ : state)
    {
        immer::vector<int> processVector;
        std::for_each(dataSource.begin(), dataSource.end(), [&](int v) { processVector = processVector.push_back(v); });
        benchmark::DoNotOptimize(processVector);
    }
}
BENCHMARK(BM_ImmerVectorForEach);

static void BM_ImmerVectorAccumulate(benchmark::State& state) {
    std::array<int, 10> dataSource{ 1,9,4,2,13, 14,9,1,3,2 };
    for (auto _ : state)
    {
        immer::vector<int> processVector =
            std::accumulate(
                dataSource.begin(),
                dataSource.end(),
                immer::vector<int>(),
                [&](auto vals, int val) { return vals.push_back(val); }
            );
        benchmark::DoNotOptimize(processVector);
    }
}
BENCHMARK(BM_ImmerVectorAccumulate);

