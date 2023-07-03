#ifndef COMMANDLINE_LIST_H
#define COMMANDLINE_LIST_H

#include <vector>

#include "Option.h"
#include "OptionEnum.h"
#include "OptionValue.h"
#include "Parser.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"

namespace Commandline {

//===----------------------------------------------------------------------===//
// Default storage class definition: external storage.  This implementation
// assumes the user will specify a variable to store the data into with the
// cl::location(x) modifier.
//
template <class DataType, class StorageClass>
class list_storage {
  StorageClass* Location = nullptr;  // Where to store the object...
  std::vector<OptionValue<DataType>> Default =
      std::vector<OptionValue<DataType>>();
  bool DefaultAssigned = false;

 public:
  list_storage() = default;

  void clear() {}

  bool setLocation(Option& O, StorageClass& L) {
    if (Location)
      return O.error("cl::location(x) specified more than once!");
    Location = &L;
    return false;
  }

  template <class T>
  void addValue(const T& V, bool initial = false) {
    assert(Location != nullptr &&
           "cl::location(...) not specified for a command "
           "line option with external storage!");
    Location->push_back(V);
    if (initial)
      Default.push_back(V);
  }

  const std::vector<OptionValue<DataType>>& getDefault() const {
    return Default;
  }

  void assignDefault() { DefaultAssigned = true; }
  void overwriteDefault() { DefaultAssigned = false; }
  bool isDefaultAssigned() { return DefaultAssigned; }
};

// Define how to hold a class type object, such as a string.
// Originally this code inherited from std::vector. In transitioning to a new
// API for command line options we should change this. The new implementation
// of this list_storage specialization implements the minimum subset of the
// std::vector API required for all the current clients.
//
// FIXME: Reduce this API to a more narrow subset of std::vector
//
template <class DataType>
class list_storage<DataType, bool> {
  std::vector<DataType> Storage;
  std::vector<OptionValue<DataType>> Default;
  bool DefaultAssigned = false;

 public:
  using iterator = typename std::vector<DataType>::iterator;

  iterator begin() { return Storage.begin(); }
  iterator end() { return Storage.end(); }

  using const_iterator = typename std::vector<DataType>::const_iterator;

  const_iterator begin() const { return Storage.begin(); }
  const_iterator end() const { return Storage.end(); }

  using size_type = typename std::vector<DataType>::size_type;

  size_type size() const { return Storage.size(); }

  bool empty() const { return Storage.empty(); }

  void push_back(const DataType& value) { Storage.push_back(value); }
  void push_back(DataType&& value) { Storage.push_back(value); }

  using reference = typename std::vector<DataType>::reference;
  using const_reference = typename std::vector<DataType>::const_reference;

  reference operator[](size_type pos) { return Storage[pos]; }
  const_reference operator[](size_type pos) const { return Storage[pos]; }

  void clear() { Storage.clear(); }

  iterator erase(const_iterator pos) { return Storage.erase(pos); }
  iterator erase(const_iterator first, const_iterator last) {
    return Storage.erase(first, last);
  }

  iterator erase(iterator pos) { return Storage.erase(pos); }
  iterator erase(iterator first, iterator last) {
    return Storage.erase(first, last);
  }

  iterator insert(const_iterator pos, const DataType& value) {
    return Storage.insert(pos, value);
  }
  iterator insert(const_iterator pos, DataType&& value) {
    return Storage.insert(pos, value);
  }

  iterator insert(iterator pos, const DataType& value) {
    return Storage.insert(pos, value);
  }
  iterator insert(iterator pos, DataType&& value) {
    return Storage.insert(pos, value);
  }

  reference front() { return Storage.front(); }
  const_reference front() const { return Storage.front(); }

  operator std::vector<DataType>&() { return Storage; }
  operator llvm::ArrayRef<DataType>() const { return Storage; }
  std::vector<DataType>* operator&() { return &Storage; }
  const std::vector<DataType>* operator&() const { return &Storage; }

  template <class T>
  void addValue(const T& V, bool initial = false) {
    Storage.push_back(V);
    if (initial)
      Default.push_back(OptionValue<DataType>(V));
  }

  const std::vector<OptionValue<DataType>>& getDefault() const {
    return Default;
  }

  void assignDefault() { DefaultAssigned = true; }
  void overwriteDefault() { DefaultAssigned = false; }
  bool isDefaultAssigned() { return DefaultAssigned; }
};

//===----------------------------------------------------------------------===//
// A list of command line options.
//
template <class DataType, class StorageClass = bool,
          class ParserClass = parser<DataType>>
class list : public Option, public list_storage<DataType, StorageClass> {
  std::vector<unsigned> Positions;
  ParserClass Parser;

  enum ValueExpected getValueExpectedFlagDefault() const override {
    return Parser.getValueExpectedFlagDefault();
  }

  void getExtraOptionNames(
      llvm::SmallVectorImpl<llvm::StringRef>& OptionNames) override {
    return Parser.getExtraOptionNames(OptionNames);
  }

  bool handleOccurrence(unsigned pos, llvm::StringRef ArgName,
                        llvm::StringRef Arg) override {
    typename ParserClass::parser_data_type Val =
        typename ParserClass::parser_data_type();
    if (list_storage<DataType, StorageClass>::isDefaultAssigned()) {
      clear();
      list_storage<DataType, StorageClass>::overwriteDefault();
    }
    if (Parser.parse(*this, ArgName, Arg, Val))
      return true;  // Parse Error!
    list_storage<DataType, StorageClass>::addValue(Val);
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

  // Unimplemented: list options don't currently store their default value.
  void printOptionValue(size_t /*GlobalWidth*/, bool /*Force*/) const override {
  }

  void setDefault() override {
    Positions.clear();
    list_storage<DataType, StorageClass>::clear();
    for (auto& Val : list_storage<DataType, StorageClass>::getDefault())
      list_storage<DataType, StorageClass>::addValue(Val.getValue());
  }

  void done() {
    addArgument();
    Parser.initialize();
  }

 public:
  // Command line options should not be copyable
  list(const list&) = delete;
  list& operator=(const list&) = delete;

  ParserClass& getParser() { return Parser; }

  unsigned getPosition(unsigned optnum) const {
    assert(optnum < this->size() && "Invalid option index");
    return Positions[optnum];
  }

  void clear() {
    Positions.clear();
    list_storage<DataType, StorageClass>::clear();
  }

  // setInitialValues - Used by the cl::list_init modifier...
  void setInitialValues(llvm::ArrayRef<DataType> Vs) {
    assert(!(list_storage<DataType, StorageClass>::isDefaultAssigned()) &&
           "Cannot have two default values");
    list_storage<DataType, StorageClass>::assignDefault();
    for (auto& Val : Vs)
      list_storage<DataType, StorageClass>::addValue(Val, true);
  }

  void setNumAdditionalVals(unsigned n) { Option::setNumAdditionalVals(n); }

  template <class... Mods>
  explicit list(const Mods&... Ms)
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

// Modifier to set the number of additional values.
struct multi_val {
  unsigned AdditionalVals;
  explicit multi_val(unsigned N) : AdditionalVals(N) {}

  template <typename D, typename S, typename P>
  void apply(list<D, S, P>& L) const {
    L.setNumAdditionalVals(AdditionalVals);
  }
};

}  // namespace Commandline

#endif  // COMMANDLINE_LIST_H