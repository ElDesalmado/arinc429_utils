﻿#pragma once

#include <climits>
#include <cstdint>
#include <ratio>
#include <tuple>
#include <type_traits>

#include <cassert>
#include <utility>

/**
 * This is a header-only utility library for ARINC 429 data protocol.
 * The goals of this library are to provide (by priority):
 * - no UB-dependent implementations
 * - consistent, easy to use and to understand API
 * - simple extension and customisation mechanism
 * - compile-time checks and type safety
 * - minimal or no runtime overhead
 * - minimal or no dependencies (std only)
 */

namespace eld
{
    namespace arinc429
    {
        namespace detail
        {
            /**
             * Helper class for pre C++17 code.
             * TODO: add macro to use std::void_t if C++17 is available
             */
            template<typename...>
            using void_t = void;

            template<class...>
            struct conjunction : std::true_type
            {
            };
            template<class B1>
            struct conjunction<B1> : B1
            {
            };
            template<class B1, class... Bn>
            struct conjunction<B1, Bn...> :
              std::conditional_t<bool(B1::value), conjunction<Bn...>, B1>
            {
            };

            template<class...>
            struct disjunction : std::false_type
            {
            };
            template<class B1>
            struct disjunction<B1> : B1
            {
            };
            template<class B1, class... Bn>
            struct disjunction<B1, Bn...> :
              std::conditional_t<bool(B1::value), B1, disjunction<Bn...>>
            {
            };

            /**
             * Calculate sum of arguments.
             * TODO: make C++11 compatible version
             * @tparam SizesT
             * @param sizes
             * @return
             */
            template<typename... ArgsT>
            constexpr std::common_type_t<ArgsT...> sum(ArgsT... args)
            {
                using sum_t = std::common_type_t<ArgsT...>;
                sum_t argsArray[]{ args... };

                sum_t sum = {};
                for (auto i : argsArray)
                {
                    sum += i;
                }
                return sum;
            }

            template<typename /*Tuple*/, typename /*Pred*/>
            struct filter;

            template<typename PlaceHolderT,
                     typename... PredArgsT,
                     typename... Ts,
                     template<typename...>
                     class Pred>
            struct filter<std::tuple<Ts...>, Pred<PlaceHolderT, PredArgsT...>>
            {
                using type = decltype(std::tuple_cat(
                    std::declval<typename std::conditional<Pred<Ts, PredArgsT...>::value,
                                                           std::tuple<Ts>,
                                                           std::tuple<>>::type>()...));
            };

            template<typename Tuple, class Pred>
            using filter_t = typename filter<Tuple, Pred>::type;

        }

        /**
         * Helper tag to define getter operator()(T&, tag_get)
         */
        struct tag_get
        {
        };

        /**
         * Helper tag to define setter operator()(const T&, tag_set)
         */
        struct tag_set
        {
        };

        namespace traits
        {
            template<typename T, typename = void>
            struct defines_lsb : std::false_type
            {
            };

            template<typename T>
            struct defines_lsb<T, detail::void_t<decltype(std::declval<T>().lsb())>> :
              std::true_type
            {
            };

            template<typename T, typename = void>
            struct defines_msb : std::false_type
            {
            };

            template<typename T>
            struct defines_msb<T, detail::void_t<decltype(std::declval<T>().msb())>> :
              std::true_type
            {
            };

            template<typename T, typename = void>
            struct defines_value_type : std::false_type
            {
            };

            template<typename T>
            struct defines_value_type<T, detail::void_t<typename T::value_type>> : std::true_type
            {
            };

            template<typename T, typename = void>
            struct defines_name_type : std::false_type
            {
            };

            template<typename T>
            struct defines_name_type<T, detail::void_t<typename T::name_type>> : std::true_type
            {
            };

            template<typename T, typename ValueType, typename = void>
            struct defines_getter : std::false_type
            {
            };

            template<typename T, typename ValueType>
            struct defines_getter<
                T,
                ValueType,
                detail::void_t<decltype(std::declval<T>()(std::declval<ValueType>(), tag_get()))>> :
              std::true_type
            {
            };

            template<typename T, typename ValueType, typename = void>
            struct defines_setter : std::false_type
            {
            };

            template<typename T, typename ValueType>
            struct defines_setter<
                T,
                ValueType,
                detail::void_t<decltype(std::declval<T>()(std::declval<const ValueType>(),
                                                          tag_set()))>> : std::true_type
            {
            };

            template<typename T>
            constexpr bool is_valid_data()
            {
                return defines_lsb<T>() &&          //
                       defines_msb<T>() &&          //
                       defines_value_type<T>() &&   //
                       defines_name_type<T>();
            }

            template<typename T>
            struct data_descriptor_traits
            {
                static_assert(is_valid_data<T>(), "Type is not a valid arinc429 data type");

                /**
                 * Get Least Significant Bit index. Index starts with 1.
                 * @return
                 */
                static constexpr size_t lsb() { return T::lsb(); }

                /**
                 * Get Most Significant Bit. Index starts with 1.
                 * @return
                 */
                static constexpr size_t msb() { return T::msb(); }

                using value_type = typename T::value_type;
                using name_type = typename T::name_type;
                using scale_factor_type = typename T::scale_factor_type;
            };

            template <typename DataDescriptorT>
            using name_type_t = typename data_descriptor_traits<DataDescriptorT>::name_type;

            using word_raw_type = uint32_t;
            constexpr size_t word_size = sizeof(word_raw_type) * CHAR_BIT;

            template <typename DataDescriptor, typename NameType>
            struct is_same_name_type : std::is_same<name_type_t<DataDescriptor>, NameType> {};

            template<typename NameType,
                     typename TupleDataDescriptors,
                     typename NotFoundPlaceholder = void>
            class get_data_descriptor
            {
                struct placeholder_t
                {
                    using name_type = void;
                };
                using filtered_t =
                    detail::filter_t<TupleDataDescriptors, is_same_name_type<placeholder_t, NameType>>;
                static_assert(!std::is_void<NotFoundPlaceholder>() ||
                                  std::tuple_size<filtered_t>() != 0,
                              "Data descriptor not found!");
                static_assert(std::tuple_size<filtered_t>() < 2,
                              "Multiple data descriptors with same name were found");

            public:
                using type = typename std::tuple_element<0, filtered_t>::type;
            };

            template<typename NameType, typename WordT, typename NotFoundPlaceholder = void>
            using get_data_descriptor_t =
                typename get_data_descriptor<NameType, WordT, NotFoundPlaceholder>::type;

            // TODO: ranges of defined bits
            //            template <template <typename ...> class WordT, typename ...
            //            DataDescriptors> struct definided_bits : std::index_sequence<>

        }

        namespace detail
        {
            template<typename T>
            void get_integral_value(T & /*dest*/,
                                    traits::word_raw_type /*wordRaw*/,
                                    size_t /*lsb*/,
                                    size_t /*msb*/,
                                    std::false_type /*is_signed*/)
            {
                // TODO: implement
            }

            template<typename T>
            void get_integral_value(T & /*dest*/,
                                    traits::word_raw_type /*wordRaw*/,
                                    size_t /*lsb*/,
                                    size_t /*msb*/,
                                    std::true_type /*is_signed*/)
            {
                // TODO: implement
            }

            template<typename T>
            void get_value(T &dest,
                           traits::word_raw_type wordRaw,
                           size_t lsb,
                           size_t msb,
                           double /*scaleFactor*/,
                           std::false_type /*is_floating_point*/)
            {
                static_assert(!std::is_floating_point<T>(), "Only integral types are expected!");
                detail::get_integral_value(dest, wordRaw, lsb, msb, std::is_signed<T>());
            }

            template<typename T>
            void get_value(T &dest,
                           traits::word_raw_type /*wordRaw*/,
                           size_t lsb,
                           size_t msb,
                           double scaleFactor,
                           std::true_type /*is_floating_point*/)
            {
                static_assert(std::is_floating_point<T>(),
                              "Only floating point types are expected!");

                // TODO: implement
            }

            template<typename DataDescriptor,
                     typename ValueType =
                         typename traits::data_descriptor_traits<DataDescriptor>::value_type>
            void get_value(ValueType &retVal,
                           traits::word_raw_type wordRaw,
                           std::false_type /*defines_getter*/)
            {
                using traits_t = traits::data_descriptor_traits<DataDescriptor>;
                using scale_factor_t = typename traits_t::scale_factor_type;
                get_value(retVal,
                          wordRaw,
                          traits_t::lsb(),
                          traits_t::msb(),
                          double(scale_factor_t::num / scale_factor_t::den),
                          std::is_floating_point<ValueType>());
            }

            template<typename DataDescriptor,
                     typename ValueType =
                         typename traits::data_descriptor_traits<DataDescriptor>::value_type>
            void get_value(ValueType &retVal,
                           traits::word_raw_type wordRaw,
                           std::true_type /*defines_getter*/)
            {
                DataDescriptor()(retVal, wordRaw, tag_get());
            }

            template<typename T>
            void set_integral_value(const T & /*value*/,
                                    traits::word_raw_type & /*wordRaw*/,
                                    size_t /*lsb*/,
                                    size_t /*msb*/,
                                    std::false_type /*is_signed*/)
            {
                // TODO: implement
            }

            template<typename T>
            void set_integral_value(const T & /*value*/,
                                    traits::word_raw_type & /*wordRaw*/,
                                    size_t /*lsb*/,
                                    size_t /*msb*/,
                                    std::true_type /*is_signed*/)
            {
                // TODO: implement
            }

            template<typename T>
            void set_value(const T &value,
                           traits::word_raw_type &wordRaw,
                           size_t lsb,
                           size_t msb,
                           double /*scaleFactor*/,
                           std::false_type /*is_floating_point*/)
            {
                static_assert(!std::is_floating_point<T>(), "Only integral types are expected!");
                detail::set_integral_value(value, wordRaw, lsb, msb, std::is_signed<T>());
            }

            template<typename T>
            void set_value(const T &value,
                           traits::word_raw_type & /*wordRaw*/,
                           size_t lsb,
                           size_t msb,
                           double scaleFactor,
                           std::true_type /*is_floating_point*/)
            {
                static_assert(std::is_floating_point<T>(),
                              "Only floating point types are expected!");

                // TODO: implement
            }

            template<typename DataDescriptor,
                     typename ValueType =
                         typename traits::data_descriptor_traits<DataDescriptor>::value_type>
            void set_value(const ValueType &value,
                           traits::word_raw_type &wordRaw,
                           std::false_type /*defines_getter*/)
            {
                using traits_t = traits::data_descriptor_traits<DataDescriptor>;
                using scale_factor_t = typename traits_t::scale_factor_type;
                set_value(value,
                          wordRaw,
                          traits_t::lsb(),
                          traits_t::msb(),
                          double(scale_factor_t::num / scale_factor_t::den),
                          std::is_floating_point<ValueType>());
            }

            template<typename DataDescriptor,
                     typename ValueType =
                         typename traits::data_descriptor_traits<DataDescriptor>::value_type>
            void set_value(const ValueType &value,
                           traits::word_raw_type &wordRaw,
                           std::true_type /*defines_getter*/)
            {
                DataDescriptor()(value, wordRaw, tag_set());
            }

        }

        template<
            typename T,
            typename = typename std::enable_if<
                detail::disjunction<std::is_floating_point<T>, std::is_integral<T>>::value>::type>
        void get_value(T &destination,
                       traits::word_raw_type wordRaw,
                       size_t lsb,
                       size_t msb,
                       double scaleFactor = 1.0)
        {
            detail::get_value(destination,
                              wordRaw,
                              lsb,
                              msb,
                              scaleFactor,
                              std::is_floating_point<T>());
        }

        template<typename DataDescriptor,
                 typename ValueType =
                     typename traits::data_descriptor_traits<DataDescriptor>::value_type>
        void get_value(ValueType &retVal, traits::word_raw_type wordRaw)
        {
            detail::get_value<DataDescriptor>(retVal,
                                              wordRaw,
                                              traits::defines_getter<DataDescriptor, ValueType>());
        }

        template<
            typename T,
            typename = typename std::enable_if<
                detail::disjunction<std::is_floating_point<T>, std::is_integral<T>>::value>::type>
        void set_value(const T &value,
                       traits::word_raw_type &wordRaw,
                       size_t lsb,
                       size_t msb,
                       double scaleFactor = 1.0)
        {
            detail::set_value(value, wordRaw, lsb, msb, scaleFactor, std::is_floating_point<T>());
        }

        template<typename DataDescriptor,
                 typename ValueType =
                     typename traits::data_descriptor_traits<DataDescriptor>::value_type>
        void set_value(const ValueType &value, traits::word_raw_type &wordRaw)
        {
            detail::get_value<DataDescriptor>(value,
                                              wordRaw,
                                              traits::defines_getter<DataDescriptor, ValueType>());
        }

        /**
         *
         * @tparam DataDescriptors
         */
        template<typename... DataDescriptors>
        class word_generic
        {
            static_assert(detail::sum(traits::data_descriptor_traits<DataDescriptors>::msb() -
                                      traits::data_descriptor_traits<DataDescriptors>::lsb()...) <=
                              traits::word_size,
                          "Size of data exceeds size of arinc 429 word!");
            // TODO: check that struct types are unique

        public:
            constexpr explicit word_generic(traits::word_raw_type rawWord)   //
              : raw_word_(rawWord)
            {
            }

            word_generic(const word_generic&) = default;
            word_generic(word_generic&&) noexcept = default;

            word_generic& operator=(const word_generic&) = default;
            word_generic& operator=(word_generic&&) noexcept = default;


            // TODO: implement get and set via index, struct type (name) and string name
            template<typename NameType,
                     typename = typename std::enable_if<true /*TODO: implement*/>::type>
            constexpr auto get()
            {
                using data_descriptor_t =
                    traits::get_data_descriptor_t<NameType, std::tuple<DataDescriptors...>>;

                typename data_descriptor_t::value_type retVal{};
                get_value<data_descriptor_t>(retVal, raw_word_);

                return retVal;
            }

            template<
                typename NameType,
                typename T,
                typename = typename std::enable_if<
                    true /*TODO: check if name exists and T corresponds to assigned ValueType*/>::
                    type>
            constexpr void set(const T &value)
            {
                using data_descriptor_t =
                    traits::get_data_descriptor_t<NameType, std::tuple<DataDescriptors...>>;

                set_value<data_descriptor_t>(value, raw_word_);
            }

            traits::word_raw_type get_raw() const
            {
                return raw_word_;
            }

            void set_raw(traits::word_raw_type rawWord)
            { raw_word_ = rawWord;
            }

            explicit operator traits::word_raw_type() const
            {
                return get_raw();
            }

            template <typename ... ArgsT>
            explicit operator word_generic<ArgsT...>() const
            {
                return word_generic<ArgsT...>(get_raw());
            }

        private:
            traits::word_raw_type raw_word_;
        };

        // TODO: add customization for data retrieval.

        /**
         * Default data descriptor. LSB and MSB indexing starts with 1.
         * @tparam LSB least significant bit.
         * @tparam MSB most significant bit
         */
        template<typename NameType,
                 size_t LSB,
                 size_t MSB,
                 typename ValueType = uint32_t,
                 typename ScaleFactorT = std::ratio<1, 1>>
        struct data_descriptor
        {
            static_assert(LSB < MSB, "Invalid bit range!");
            static_assert(MSB <= traits::word_size, "MSB exceeds maximum index");

            /**
             * Get Least Significant Bit index. Index starts with 1.
             * @return
             */
            static constexpr size_t lsb() { return LSB; }

            /**
             * Get Most Significant Bit. Index starts with 1.
             * @return
             */
            static constexpr size_t msb() { return MSB; }

            using value_type = ValueType;
            using name_type = NameType;
            using scale_factor_type = ScaleFactorT;
        };

    }
}