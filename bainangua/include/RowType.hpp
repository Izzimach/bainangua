﻿#pragma once

#include <boost/hana/assert.hpp>
#include <boost/hana/contains.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/set.hpp>
#include <boost/hana/at_key.hpp>
#include <boost/hana/map.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>
#include <boost/hana/string.hpp>

#include <string>

namespace RowType {

	void testRowTypes();

	template <typename Row>
	constexpr auto getRowField(Row r, const char* fieldName) {
		return boost::hana::at_key(r, BOOST_HANA_STRING(fieldName));
	}

	template <typename Row, boost::hana::string FieldName, typename FieldType>
	concept has_named_field = requires (Row s) {
		{ boost::hana::at_key(s, FieldName) } -> std::convertible_to<FieldType>;
	};

	// If the type of a field is hard to derive or access, you may not be able to use
	// has_named_field<>.  In that case you can still get some error checking of your row type by requiring a field of
	// a specific name with an unconstrained type.
	template <typename Row, boost::hana::string FieldName>
	concept has_namedonly_field = requires (Row s) {
		boost::hana::find(s, FieldName).value();
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

		
	struct RowWrapperTag {};
	struct RowFunctionTag {};

	template <typename RowWrapper, typename RowFunction>
	concept isRowWrapper =
		std::is_same_v<typename RowWrapper::row_tag, RowWrapperTag>&&
		std::is_same_v<typename RowFunction::row_tag, RowFunctionTag>;

	template <typename RowWrapper1, typename RowWrapper2>
	concept isRowWrapperCompose =
		std::is_same_v<typename RowWrapper1::row_tag, RowWrapperTag>&&
		std::is_same_v<typename RowWrapper2::row_tag, RowWrapperTag>;

	template <typename RowWrapper, typename RowFunction>
	struct ComposedRowFunction {
		ComposedRowFunction(RowWrapper w, RowFunction f) : w_(w), f_(f) {}

		RowWrapper w_;
		RowFunction f_;

		using row_tag = RowWrapperTag;
		using return_type = RowWrapper::template return_type_transformer<RowFunction::return_type>;

		template <typename Row>
		constexpr return_type applyRow(Row r) { return w_.wrapRowFunction(f_, r); }
	};

	template <typename RowWrapper1, typename RowWrapper2>
	struct ComposedRowWrappers {
		ComposedRowWrappers(RowWrapper1 w1, RowWrapper2 w2) : w1_(w1), w2_(w2) {}

		RowWrapper1 w1_;
		RowWrapper2 w2_;

		using row_tag = RowWrapperTag;

		template <typename WrappedReturnType>
		using return_type_transformer = RowWrapper1::template return_type_transformer<RowWrapper2::template return_type_transformer<WrappedReturnType>>;

		template <typename RowFunction, typename Row>
		constexpr return_type_transformer<typename RowFunction::return_type> wrapRowFunction(RowFunction f, Row r) {
			return w1_.wrapRowFunction(ComposedRowFunction<RowWrapper2, RowFunction>(w2_, f), r);
		}
	};
	
	template <typename RetType, typename TemplateLambda>
	struct LambdaRowFunction {
		LambdaRowFunction(TemplateLambda l) : l_(l) {}

		TemplateLambda l_;

		using row_tag = RowType::RowFunctionTag;
		using return_type = RetType;

		template <typename Row>
		constexpr RetType applyRow(Row r) {
			return l_(r);
		}
	};

	template <typename RetType, typename TemplateLambda>
	auto RowWrapLambda(TemplateLambda l) -> LambdaRowFunction<RetType, TemplateLambda> {
		return LambdaRowFunction<RetType, TemplateLambda>(l);
	}



}

// pipe composition is global, we'll avoid overload collision with concepts

template <typename RowWrapper, typename RowFunction>
	requires RowType::isRowWrapper<RowWrapper, RowFunction>
constexpr auto operator | (RowWrapper w, RowFunction f) { return RowType::ComposedRowFunction<RowWrapper, RowFunction>(w,f); };


template <typename RowWrapper1, typename RowWrapper2>
	requires RowType::isRowWrapperCompose<RowWrapper1, RowWrapper2>
constexpr auto operator | (RowWrapper1 w1, RowWrapper2 w2) { return RowType::ComposedRowWrappers<RowWrapper1, RowWrapper2>(w1,w2); };

