#ifndef LIBNOP_INCLUDE_NOP_BASE_VARIANT_H_
#define LIBNOP_INCLUDE_NOP_BASE_VARIANT_H_

#include <nop/base/encoding.h>
#include <nop/types/variant.h>

namespace nop {

//
// Variant<Ts...> encoding format:
//
// +-----+---------+-----------+
// | VAR | INT32:I | ELEMENT I |
// +-----+---------+-----------+
//
// Elements are expected to be valid encodings for their element type.
//
// EmptyVariant encoding format:
//
// +-----+
// | NIL |
// +-----+
//
// Therefore a Variant in the empty state has this specific encoding:
//
// +-----+----+-----+
// | VAR | -1 | NIL |
// +-----+----+-----+
//

template <>
struct Encoding<EmptyVariant> : EncodingIO<EmptyVariant> {
  static constexpr EncodingByte Prefix(EmptyVariant /*value*/) {
    return EncodingByte::Nil;
  }

  static constexpr std::size_t Size(EmptyVariant value) {
    return BaseEncodingSize(Prefix(value));
  }

  static constexpr bool Match(EncodingByte prefix) {
    return prefix == EncodingByte::Nil;
  }

  template <typename Writer>
  static Status<void> WritePayload(EncodingByte /*prefix*/,
                                   EmptyVariant /*value*/, Writer* /*writer*/) {
    return {};
  }

  template <typename Reader>
  static Status<void> ReadPayload(EncodingByte /*prefix*/,
                                  EmptyVariant* /*value*/, Reader* /*reader*/) {
    return {};
  }
};

template <typename... Ts>
struct Encoding<Variant<Ts...>> : EncodingIO<Variant<Ts...>> {
  using Type = Variant<Ts...>;

  static constexpr EncodingByte Prefix(const Type& /*value*/) {
    return EncodingByte::Variant;
  }

  static constexpr std::size_t Size(const Type& value) {
    return BaseEncodingSize(Prefix(value)) +
           Encoding<std::int32_t>::Size(value.index()) +
           value.Visit([](const auto& element) {
             using Element = typename std::decay<decltype(element)>::type;
             return Encoding<Element>::Size(element);
           });
  }

  static constexpr bool Match(EncodingByte prefix) {
    return prefix == EncodingByte::Variant;
  }

  template <typename Writer>
  static Status<void> WritePayload(EncodingByte /*prefix*/, const Type& value,
                                   Writer* writer) {
    auto status = Encoding<std::int32_t>::Write(value.index(), writer);
    if (!status)
      return status;

    return value.Visit([writer](const auto& element) {
      using Element = typename std::decay<decltype(element)>::type;
      return Encoding<Element>::Write(element, writer);
    });
  }

  template <typename Reader>
  static Status<void> ReadPayload(EncodingByte /*prefix*/, Type* value,
                                  Reader* reader) {
    std::int32_t type;
    auto status = Encoding<std::int32_t>::Read(&type, reader);
    if (!status) {
      return status;
    } else if (type < Type::kEmptyIndex ||
               type >= static_cast<std::int32_t>(sizeof...(Ts))) {
      return ErrorStatus(EPROTO);
    }

    value->Become(type);

    return value->Visit([reader](auto&& element) {
      using Element = typename std::decay<decltype(element)>::type;
      return Encoding<Element>::Read(&element, reader);
    });
  }
};

}  // namespace nop

#endif  // LIBNOP_INCLUDE_NOP_BASE_VARIANT_H_
