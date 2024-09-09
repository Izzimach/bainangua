

#include <boost/hana/assert.hpp>
#include <boost/hana/contains.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/set.hpp>
#include <boost/hana/at_key.hpp>
#include <boost/hana/map.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>
#include <boost/hana/string.hpp>

#include <fmt/format.h>

namespace RowType {

	template <typename Row, boost::hana::string FieldName, typename FieldType>
	concept has_named_field = requires (Row s) {
		{ boost::hana::at_key(s, FieldName) } -> std::convertible_to<FieldType>;
	};

	//
	// from https://stackoverflow.com/questions/67021626/how-do-you-define-user-defined-string-literal-operator-templates-in-c17
	//

	template<std::size_t N>
	struct MakeArray
	{
		std::array<char, N> data;
		constexpr auto get() const { return data; }

		template <std::size_t... Is>
		constexpr MakeArray(const char(&arr)[N], std::integer_sequence<std::size_t, Is...>) : data{ arr[Is]... } {}

		constexpr MakeArray(char const(&arr)[N]) : MakeArray(arr, std::make_integer_sequence<std::size_t, N>())
		{}
	};

	//
	// WTF
	//
	template <typename S, std::size_t ...N>
	constexpr boost::hana::string<S::get()[N]...>
		prepare_impl2(S, std::index_sequence<N...>)
	{
		return {};
	}

	template <typename S>
	constexpr decltype(auto) prepare2(S s) {
		return prepare_impl2(s,
			std::make_index_sequence<sizeof(S::get()) - 1>{});
	}

	template<MakeArray A>
	constexpr auto operator"" _field()
	{
		return prepare2([]() {                              \
			struct tmp {
			static constexpr decltype(auto) get() { return A.data; }
		};
		return tmp{};
			}());
	}


	template <boost::hana::string FieldName, typename T>
		requires has_named_field<T, FieldName, std::string>
	constexpr auto getRowField(const T& s)
	{
		return boost::hana::at_key(s, FieldName);
	}

}