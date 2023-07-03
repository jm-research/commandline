#ifndef COMMANDLINE_BITS_H
#define COMMANDLINE_BITS_H

#include "Option.h"
#include "Parser.h"

#include <cassert>
#include <climits>

namespace Commandline {

//===----------------------------------------------------------------------===//
// Default storage class definition: external storage.  This implementation
// assumes the user will specify a variable to store the data into with the
// cl::location(x) modifier.
//
template <class DataType, class StorageClass>
class bits_storage {
  unsigned* Location = nullptr;  // Where to store the bits...

  template <class T>
  static unsigned Bit(const T& V) {
    unsigned BitPos = static_cast<unsigned>(V);
    assert(BitPos < sizeof(unsigned) * CHAR_BIT &&
           "enum exceeds width of bit vector!");
    return 1 << BitPos;
  }

 public:
  bits_storage() = default;

  bool setLocation(Option& O, unsigned& L) {
    if (Location)
      return O.error("cl::location(x) specified more than once!");
    Location = &L;
    return false;
  }

  template <class T>
  void addValue(const T& V) {
    assert(Location != nullptr &&
           "cl::location(...) not specified for a command "
           "line option with external storage!");
    *Location |= Bit(V);
  }

  unsigned getBits() { return *Location; }

  void clear() {
    if (Location)
      *Location = 0;
  }

  template <class T>
  bool isSet(const T& V) {
    return (*Location & Bit(V)) != 0;
  }
};

// Define how to hold bits.  Since we can inherit from a class, we do so.
// This makes us exactly compatible with the bits in all cases that it is used.
//
template <class DataType>
class bits_storage<DataType, bool> {
  unsigned Bits{0};  // Where to store the bits...

  template <class T>
  static unsigned Bit(const T& V) {
    unsigned BitPos = static_cast<unsigned>(V);
    assert(BitPos < sizeof(unsigned) * CHAR_BIT &&
           "enum exceeds width of bit vector!");
    return 1 << BitPos;
  }

 public:
  template <class T>
  void addValue(const T& V) {
    Bits |= Bit(V);
  }

  unsigned getBits() { return Bits; }

  void clear() { Bits = 0; }

  template <class T>
  bool isSet(const T& V) {
    return (Bits & Bit(V)) != 0;
  }
};

//===----------------------------------------------------------------------===//
// A bit vector of command options.
//
template <class DataType, class Storage = bool,
          class ParserClass = parser<DataType>>
class bits : public Option, public bits_storage<DataType, Storage> {
  std::vector<unsigned> Positions;
  ParserClass Parser;

  enum ValueExpected getValueExpectedFlagDefault() const override {
    return Parser.getValueExpectedFlagDefault();
  }

  void getExtraOptionNames(llvm::SmallVectorImpl<llvm::StringRef>& OptionNames) override {
    return Parser.getExtraOptionNames(OptionNames);
  }

  bool handleOccurrence(unsigned pos, llvm::StringRef ArgName,
                        llvm::StringRef Arg) override {
    typename ParserClass::parser_data_type Val =
        typename ParserClass::parser_data_type();
    if (Parser.parse(*this, ArgName, Arg, Val))
      return true;  // Parse Error!
    this->addValue(Val);
    setPosition(pos);
    Positions.push_back(pos);
    Callback(Val);
    return false;
  }

  // Forward printing stuff to the parser...
  size_t getOptionWidth() const override {
    return Parser.getOptionWidth(*this);
  }

  void printOptionInfo(size_t GlobalWidth) const override {
    Parser.printOptionInfo(*this, GlobalWidth);
  }

  // Unimplemented: bits options don't currently store their default values.
  void printOptionValue(size_t /*GlobalWidth*/, bool /*Force*/) const override {
  }

  void setDefault() override { bits_storage<DataType, Storage>::clear(); }

  void done() {
    addArgument();
    Parser.initialize();
  }

 public:
  // Command line options should not be copyable
  bits(const bits&) = delete;
  bits& operator=(const bits&) = delete;

  ParserClass& getParser() { return Parser; }

  unsigned getPosition(unsigned optnum) const {
    assert(optnum < this->size() && "Invalid option index");
    return Positions[optnum];
  }

  template <class... Mods>
  explicit bits(const Mods&... Ms)
      : Option(ZeroOrMore, NotHidden), Parser(*this) {
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

}  // namespace Commandline

#endif  // COMMANDLINE_BITS_H