#ifndef COMMANDLINE_OPT_H
#define COMMANDLINE_OPT_H

#include "Parser.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"

namespace Commandline {

//===----------------------------------------------------------------------===//
// Default storage class definition: external storage.  This implementation
// assumes the user will specify a variable to store the data into with the
// location(x) modifier.
//
template <class DataType, bool ExternalStorage, bool isClass>
class opt_storage {
  DataType* Location = nullptr;  // Where to store the object...
  OptionValue<DataType> Default;

  void check_location() const {
    assert(Location &&
           "location(...) not specified for a command "
           "line option with external storage, "
           "or init specified before cl::location()!!");
  }

 public:
  opt_storage() = default;

  bool setLocation(Option& O, DataType& L) {
    if (Location)
      return O.error("cl::location(x) specified more than once!");
    Location = &L;
    Default = L;
    return false;
  }

  template <class T>
  void setValue(const T& V, bool initial = false) {
    check_location();
    *Location = V;
    if (initial)
      Default = V;
  }

  DataType& getValue() {
    check_location();
    return *Location;
  }
  const DataType& getValue() const {
    check_location();
    return *Location;
  }

  operator DataType() const { return this->getValue(); }

  const OptionValue<DataType>& getDefault() const { return Default; }
};

// Define how to hold a class type object, such as a string.  Since we can
// inherit from a class, we do so.  This makes us exactly compatible with the
// object in all cases that it is used.
//
template <class DataType>
class opt_storage<DataType, false, true> : public DataType {
 public:
  OptionValue<DataType> Default;

  template <class T>
  void setValue(const T& V, bool initial = false) {
    DataType::operator=(V);
    if (initial)
      Default = V;
  }

  DataType& getValue() { return *this; }
  const DataType& getValue() const { return *this; }

  const OptionValue<DataType>& getDefault() const { return Default; }
};

// Define a partial specialization to handle things we cannot inherit from.  In
// this case, we store an instance through containment, and overload operators
// to get at the value.
//
template <class DataType>
class opt_storage<DataType, false, false> {
 public:
  DataType Value;
  OptionValue<DataType> Default;

  // Make sure we initialize the value with the default constructor for the
  // type.
  opt_storage() : Value(DataType()), Default() {}

  template <class T>
  void setValue(const T& V, bool initial = false) {
    Value = V;
    if (initial)
      Default = V;
  }
  DataType& getValue() { return Value; }
  DataType getValue() const { return Value; }

  const OptionValue<DataType>& getDefault() const { return Default; }

  operator DataType() const { return getValue(); }

  // If the datatype is a pointer, support -> on it.
  DataType operator->() const { return Value; }
};

//===----------------------------------------------------------------------===//
// A scalar command line option.
//
template <class DataType, bool ExternalStorage = false,
          class ParserClass = parser<DataType>>
class opt
    : public Option,
      public opt_storage<DataType, ExternalStorage, std::is_class_v<DataType>> {
  ParserClass Parser;

  bool handleOccurrence(unsigned pos, llvm::StringRef ArgName,
                        llvm::StringRef Arg) override {
    typename ParserClass::parser_data_type Val =
        typename ParserClass::parser_data_type();
    if (Parser.parse(*this, ArgName, Arg, Val))
      return true;  // Parse error!
    this->setValue(Val);
    this->setPosition(pos);
    Callback(Val);
    return false;
  }

  enum ValueExpected getValueExpectedFlagDefault() const override {
    return Parser.getValueExpectedFlagDefault();
  }

  void getExtraOptionNames(
      llvm::SmallVectorImpl<llvm::StringRef>& OptionNames) override {
    return Parser.getExtraOptionNames(OptionNames);
  }

  // Forward printing stuff to the parser...
  size_t getOptionWidth() const override {
    return Parser.getOptionWidth(*this);
  }

  void printOptionInfo(size_t GlobalWidth) const override {
    Parser.printOptionInfo(*this, GlobalWidth);
  }

  void printOptionValue(size_t GlobalWidth, bool Force) const override {
    if (Force || this->getDefault().compare(this->getValue())) {
      printOptionDiff<ParserClass>(*this, Parser, this->getValue(),
                                   this->getDefault(), GlobalWidth);
    }
  }

  template <class T, class = std::enable_if_t<std::is_assignable_v<T&, T>>>
  void setDefaultImpl() {
    const OptionValue<DataType>& V = this->getDefault();
    if (V.hasValue())
      this->setValue(V.getValue());
    else
      this->setValue(T());
  }

  template <class T, class = std::enable_if_t<!std::is_assignable_v<T&, T>>>
  void setDefaultImpl(...) {}

  void setDefault() override { setDefaultImpl<DataType>(); }

  void done() {
    addArgument();
    Parser.initialize();
  }

 public:
  // Command line options should not be copyable
  opt(const opt&) = delete;
  opt& operator=(const opt&) = delete;

  // setInitialValue - Used by the cl::init modifier...
  void setInitialValue(const DataType& V) { this->setValue(V, true); }

  ParserClass& getParser() { return Parser; }

  template <class T>
  DataType& operator=(const T& Val) {
    this->setValue(Val);
    Callback(Val);
    return this->getValue();
  }

  template <class... Mods>
  explicit opt(const Mods&... Ms) : Option(Optional, NotHidden), Parser(*this) {
    apply(this, Ms...);
    done();
  }

  void setCallback(
      std::function<void(const typename ParserClass::parser_data_type&)> CB) {
    Callback = CB;
  }

  std::function<void(const typename ParserClass::parser_data_type&)> Callback =
      [](const typename ParserClass::parser_data_type&) {};
};

extern template class opt<unsigned>;
extern template class opt<int>;
extern template class opt<std::string>;
extern template class opt<char>;
extern template class opt<bool>;

}  // namespace Commandline

#endif  // COMMANDLINE_OPT_H