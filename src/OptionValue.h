#ifndef COMMANDLINE_OPTIONVALUE_H
#define COMMANDLINE_OPTIONVALUE_H

#include <string>

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/ErrorHandling.h"

namespace Commandline {

// Support value comparison outside the template.
struct GenericOptionValue {
  virtual auto compare(const GenericOptionValue& V) const -> bool = 0;

 protected:
  GenericOptionValue() = default;
  GenericOptionValue(const GenericOptionValue&) = default;
  auto operator=(const GenericOptionValue&) -> GenericOptionValue& = default;
  ~GenericOptionValue() = default;

 private:
  virtual void anchor();
};

template <class DataType>
struct OptionValue;

// The default value safely does nothing. Option value printing is only
// best-effort.
template <class DataType, bool IsClass>
struct OptionValueBase : public GenericOptionValue {
  // Temporary storage for argument passing.
  using WrapperType = OptionValue<DataType>;

  auto hasValue() const -> bool { return false; }

  auto getValue() const -> const DataType& {
    llvm_unreachable("no default value");
  }

  // Some options may take their value from a different data type.
  template <class DT>
  void setValue(const DT& /*V*/) {}

  auto compare(const DataType& /*V*/) const -> bool { return false; }

  auto compare(const GenericOptionValue& /*V*/) const -> bool override {
    return false;
  }

 protected:
  ~OptionValueBase() = default;
};

// Simple copy of the option value.
template <class DataType>
class OptionValueCopy : public GenericOptionValue {
  DataType Value;
  bool Valid = false;

 protected:
  OptionValueCopy(const OptionValueCopy&) = default;
  auto operator=(const OptionValueCopy&) -> OptionValueCopy& = default;
  ~OptionValueCopy() = default;

 public:
  OptionValueCopy() = default;

  auto hasValue() const -> bool { return Valid; }

  auto getValue() const -> const DataType& {
    assert(Valid && "invalid option value");
    return Value;
  }

  void setValue(const DataType& v) {
    Valid = true;
    Value = v;
  }

  auto compare(const DataType& v) const -> bool {
    return Valid && (Value != v);
  }

  auto compare(const GenericOptionValue& v) const -> bool override {
    const auto& vc = static_cast<const OptionValueCopy<DataType>&>(v);
    if (!vc.hasValue()) {
      return false;
    }
    return compare(vc.getValue());
  }
};

// Non-class option values.
template <class DataType>
struct OptionValueBase<DataType, false> : OptionValueCopy<DataType> {
  using WrapperType = DataType;

 protected:
  OptionValueBase() = default;
  OptionValueBase(const OptionValueBase&) = default;
  auto operator=(const OptionValueBase&) -> OptionValueBase& = default;
  ~OptionValueBase() = default;
};

// Top-level option class.
template <class DataType>
struct OptionValue final
    : OptionValueBase<DataType, std::is_class_v<DataType>> {
  OptionValue() = default;

  OptionValue(const DataType& v) { this->setValue(v); }

  // Some options may take their value from a different data type.
  template <class DT>
  auto operator=(const DT& v) -> OptionValue<DataType>& {
    this->setValue(v);
    return *this;
  }
};

// Other safe-to-copy-by-value common option types.
enum boolOrDefault { BOU_UNSET, BOU_TRUE, BOU_FALSE };
template <>
struct OptionValue<boolOrDefault> final : OptionValueCopy<boolOrDefault> {
  using WrapperType = boolOrDefault;

  OptionValue() = default;

  OptionValue(const boolOrDefault& v) { this->setValue(v); }

  auto operator=(const boolOrDefault& v) -> OptionValue<boolOrDefault>& {
    setValue(v);
    return *this;
  }

 private:
  void anchor() override;
};

template <>
struct OptionValue<std::string> final : OptionValueCopy<std::string> {
  using WrapperType = llvm::StringRef;

  OptionValue() = default;

  OptionValue(const std::string& v) { this->setValue(v); }

  auto operator=(const std::string& v) -> OptionValue<std::string>& {
    setValue(v);
    return *this;
  }

 private:
  void anchor() override;
};

//===----------------------------------------------------------------------===//
// Enum valued command line option
//

// This represents a single enum value, using "int" as the underlying type.
struct OptionEnumValue {
  llvm::StringRef Name;
  int Value;
  llvm::StringRef Description;
};

#define clEnumVal(ENUMVAL, DESC) \
  OptionEnumValue { #ENUMVAL, int(ENUMVAL), DESC }
#define clEnumValN(ENUMVAL, FLAGNAME, DESC) \
  OptionEnumValue { FLAGNAME, int(ENUMVAL), DESC }

// For custom data types, allow specifying a group of values together as the
// values that go into the mapping that the option handler uses.
//
class ValuesClass {
  // Use a vector instead of a map, because the lists should be short,
  // the overhead is less, and most importantly, it keeps them in the order
  // inserted so we can print our option out nicely.
  llvm::SmallVector<OptionEnumValue, 4> Values;

 public:
  ValuesClass(std::initializer_list<OptionEnumValue> options)
      : Values(options) {}

  template <class Opt>
  void apply(Opt& o) const {
    for (const auto& value : Values)
      o.getParser().addLiteralOption(value.Name, value.Value,
                                     value.Description);
  }
};

/// Helper to build a ValuesClass by forwarding a variable number of arguments
/// as an initializer list to the ValuesClass constructor.
template <typename... OptsTy>
auto values(OptsTy... options) -> ValuesClass {
  return ValuesClass({options...});
}

}  // namespace Commandline

#endif  // COMMANDLINE_OPTIONVALUE_H