#ifndef COMMANDLINE_PARSER_H
#define COMMANDLINE_PARSER_H

#include "Option.h"
#include "OptionValue.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Twine.h"

namespace Commandline {

//===----------------------------------------------------------------------===//
// Parameterizable parser for different data types. By default, known data types
// (string, int, bool) have specialized parsers, that do what you would expect.
// The default parser, used for data types that are not built-in, uses a mapping
// table to map specific options to values, which is used, among other things,
// to handle enum types.

//--------------------------------------------------
// This class holds all the non-generic code that we do not need replicated for
// every instance of the generic parser.  This also allows us to put stuff into
// CommandLine.cpp
//
class generic_parser_base {
 protected:
  class GenericOptionInfo {
   public:
    GenericOptionInfo(llvm::StringRef name, llvm::StringRef help_str)
        : Name(name), HelpStr(help_str) {}
    llvm::StringRef Name;
    llvm::StringRef HelpStr;
  };

 public:
  generic_parser_base(Option& o) : Owner(o) {}

  virtual ~generic_parser_base() = default;
  // Base class should have virtual-destructor

  // Virtual function implemented by generic subclass to indicate how many
  // entries are in Values.
  //
  virtual auto getNumOptions() const -> unsigned = 0;

  // Return option name N.
  virtual auto getOption(unsigned n) const -> llvm::StringRef = 0;

  // Return description N
  virtual auto getDescription(unsigned n) const -> llvm::StringRef = 0;

  // Return the width of the option tag for printing...
  virtual auto getOptionWidth(const Option& o) const -> size_t;

  virtual auto getOptionValue(unsigned n) const
      -> const GenericOptionValue& = 0;

  // Print out information about this option. The to-be-maintained width is
  // specified.
  //
  virtual void printOptionInfo(const Option& o, size_t global_width) const;

  void printGenericOptionDiff(const Option& o, const GenericOptionValue& v,
                              const GenericOptionValue& Default,
                              size_t global_width) const;

  // Print the value of an option and it's default.
  //
  // Template definition ensures that the option and default have the same
  // DataType (via the same AnyOptionValue).
  template <class AnyOptionValue>
  void printOptionDiff(const Option& o, const AnyOptionValue& v,
                       const AnyOptionValue& Default,
                       size_t global_width) const {
    printGenericOptionDiff(o, v, Default, global_width);
  }

  void initialize() {}

  void getExtraOptionNames(
      llvm::SmallVectorImpl<llvm::StringRef>& option_names) {
    // If there has been no argstr specified, that means that we need to add an
    // argument for every possible option.  This ensures that our options are
    // vectored to us.
    if (!Owner.hasArgStr()) {
      for (unsigned i = 0, e = getNumOptions(); i != e; ++i) {
        option_names.push_back(getOption(i));
      }
    }
  }

  auto getValueExpectedFlagDefault() const -> enum ValueExpected {
    // If there is an ArgStr specified, then we are of the form:
    //
    //    -opt=O2   or   -opt O2  or  -optO2
    //
    // In which case, the value is required.  Otherwise if an arg str has not
    // been specified, we are of the form:
    //
    //    -O2 or O2 or -la (where -l and -a are separate options)
    //
    // If this is the case, we cannot allow a value.
    //
    if (Owner.hasArgStr()) return ValueRequired; else return ValueDisallowed;
  }

  // Return the option number corresponding to the specified
  // argument string.  If the option is not found, getNumOptions() is returned.
  //
  auto findOption(llvm::StringRef name) -> unsigned;

 protected:
  Option& Owner;
};

// Default parser implementation - This implementation depends on having a
// mapping of recognized options to values of some sort.  In addition to this,
// each entry in the mapping also tracks a help message that is printed with the
// command line option for -help.  Because this is a simple mapping parser, the
// data type can be any unsupported type.
//
template <class DataType>
class parser : public generic_parser_base {
 protected:
  class OptionInfo : public GenericOptionInfo {
   public:
    OptionInfo(llvm::StringRef name, DataType v, llvm::StringRef help_str)
        : GenericOptionInfo(name, help_str), V(v) {}

    OptionValue<DataType> V;
  };
  llvm::SmallVector<OptionInfo, 8> Values;

 public:
  parser(Option& o) : generic_parser_base(o) {}

  using parser_data_type = DataType;

  // Implement virtual functions needed by generic_parser_base
  auto getNumOptions() const -> unsigned override {
    return unsigned(Values.size());
  }
  auto getOption(unsigned n) const -> llvm::StringRef override {
    return Values[n].Name;
  }
  auto getDescription(unsigned n) const -> llvm::StringRef override {
    return Values[n].HelpStr;
  }

  // Return the value of option name N.
  auto getOptionValue(unsigned n) const -> const GenericOptionValue& override {
    return Values[n].V;
  }

  // Return true on error.
  auto parse(Option& o, llvm::StringRef arg_name, llvm::StringRef arg,
             DataType& v) -> bool {
    llvm::StringRef arg_val;
    if (Owner.hasArgStr()) {
      arg_val = arg;
    } else {
      arg_val = arg_name;
    }

    for (size_t i = 0, e = Values.size(); i != e; ++i) {
      if (Values[i].Name == arg_val) {
        v = Values[i].V.getValue();
        return false;
      }
    }

    return o.error("Cannot find option named '" + arg_val + "'!");
  }

  /// Add an entry to the mapping table.
  ///
  template <class DT>
  void addLiteralOption(llvm::StringRef name, const DT& v,
                        llvm::StringRef help_str) {
    assert(findOption(name) == Values.size() && "Option already exists!");
    OptionInfo x(name, static_cast<DataType>(v), help_str);
    Values.push_back(x);
    AddLiteralOption(Owner, name);
  }

  /// Remove the specified option.
  ///
  void removeLiteralOption(llvm::StringRef name) {
    unsigned n = findOption(name);
    assert(n != Values.size() && "Option not found!");
    Values.erase(Values.begin() + n);
  }
};

//--------------------------------------------------
// Super class of parsers to provide boilerplate code
//
class basic_parser_impl {  // non-template implementation of basic_parser<t>
 public:
  basic_parser_impl(Option& /*unused*/) {}

  virtual ~basic_parser_impl() = default;

  auto getValueExpectedFlagDefault() const -> enum ValueExpected {
    return ValueRequired;
  }

  void getExtraOptionNames(llvm::SmallVectorImpl<llvm::StringRef>& /*unused*/) {
  }

  void initialize() {}

  // Return the width of the option tag for printing...
  auto getOptionWidth(const Option& o) const -> size_t;

  // Print out information about this option. The to-be-maintained width is
  // specified.
  //
  void printOptionInfo(const Option& o, size_t global_width) const;

  // Print a placeholder for options that don't yet support printOptionDiff().
  void printOptionNoValue(const Option& o, size_t global_width) const;

  // Overload in subclass to provide a better default value.
  virtual auto getValueName() const -> llvm::StringRef { return "value"; }

  // An out-of-line virtual method to provide a 'home' for this class.
  virtual void anchor();

 protected:
  // A helper for basic_parser::printOptionDiff.
  void printOptionName(const Option& o, size_t global_width) const;
};

// The real basic parser is just a template wrapper that provides a typedef for
// the provided data type.
//
template <class DataType>
class basic_parser : public basic_parser_impl {
 public:
  using parser_data_type = DataType;
  using OptVal = OptionValue<DataType>;

  basic_parser(Option& o) : basic_parser_impl(o) {}
};

//--------------------------------------------------

extern template class basic_parser<bool>;

template <>
class parser<bool> : public basic_parser<bool> {
 public:
  parser(Option& o) : basic_parser(o) {}

  // Return true on error.
  auto parse(Option& o, llvm::StringRef arg_name, llvm::StringRef arg,
             bool& val) -> bool;

  void initialize() {}

  auto getValueExpectedFlagDefault() const -> enum ValueExpected {
    return ValueOptional;
  }

  // Do not print =<value> at all.
  auto getValueName() const -> llvm::StringRef override {
    return llvm::StringRef();
  }

  void printOptionDiff(const Option& o, bool v, OptVal Default,
                       size_t global_width) const;

  // An out-of-line virtual method to provide a 'home' for this class.
  void anchor() override;
};

//--------------------------------------------------

extern template class basic_parser<boolOrDefault>;

template <>
class parser<boolOrDefault> : public basic_parser<boolOrDefault> {
 public:
  parser(Option& o) : basic_parser(o) {}

  // Return true on error.
  auto parse(Option& o, llvm::StringRef arg_name, llvm::StringRef arg,
             boolOrDefault& val) -> bool;

  auto getValueExpectedFlagDefault() const -> enum ValueExpected {
    return ValueOptional;
  }

  // Do not print =<value> at all.
  auto getValueName() const -> llvm::StringRef override {
    return llvm::StringRef();
  }

  void printOptionDiff(const Option& o, boolOrDefault v, OptVal Default,
                       size_t global_width) const;

  // An out-of-line virtual method to provide a 'home' for this class.
  void anchor() override;
};

//--------------------------------------------------

extern template class basic_parser<int>;

template <>
class parser<int> : public basic_parser<int> {
 public:
  parser(Option& o) : basic_parser(o) {}

  // Return true on error.
  auto parse(Option& o, llvm::StringRef arg_name, llvm::StringRef arg, int& val)
      -> bool;

  // Overload in subclass to provide a better default value.
  auto getValueName() const -> llvm::StringRef override { return "int"; }

  void printOptionDiff(const Option& o, int v, OptVal Default,
                       size_t global_width) const;

  // An out-of-line virtual method to provide a 'home' for this class.
  void anchor() override;
};

//--------------------------------------------------

extern template class basic_parser<long>;

template <>
class parser<long> final : public basic_parser<long> {
 public:
  parser(Option& o) : basic_parser(o) {}

  // Return true on error.
  auto parse(Option& o, llvm::StringRef arg_name, llvm::StringRef arg,
             long& val) -> bool;

  // Overload in subclass to provide a better default value.
  auto getValueName() const -> llvm::StringRef override { return "long"; }

  void printOptionDiff(const Option& o, long v, OptVal Default,
                       size_t global_width) const;

  // An out-of-line virtual method to provide a 'home' for this class.
  void anchor() override;
};

//--------------------------------------------------

extern template class basic_parser<long long>;

template <>
class parser<long long> : public basic_parser<long long> {
 public:
  parser(Option& o) : basic_parser(o) {}

  // Return true on error.
  auto parse(Option& o, llvm::StringRef arg_name, llvm::StringRef arg,
             long long& val) -> bool;

  // Overload in subclass to provide a better default value.
  auto getValueName() const -> llvm::StringRef override { return "long"; }

  void printOptionDiff(const Option& o, long long v, OptVal Default,
                       size_t global_width) const;

  // An out-of-line virtual method to provide a 'home' for this class.
  void anchor() override;
};

//--------------------------------------------------

extern template class basic_parser<unsigned>;

template <>
class parser<unsigned> : public basic_parser<unsigned> {
 public:
  parser(Option& o) : basic_parser(o) {}

  // Return true on error.
  auto parse(Option& o, llvm::StringRef arg_name, llvm::StringRef arg,
             unsigned& val) -> bool;

  // Overload in subclass to provide a better default value.
  auto getValueName() const -> llvm::StringRef override { return "uint"; }

  void printOptionDiff(const Option& o, unsigned v, OptVal Default,
                       size_t global_width) const;

  // An out-of-line virtual method to provide a 'home' for this class.
  void anchor() override;
};

//--------------------------------------------------

extern template class basic_parser<unsigned long>;

template <>
class parser<unsigned long> final : public basic_parser<unsigned long> {
 public:
  parser(Option& o) : basic_parser(o) {}

  // Return true on error.
  auto parse(Option& o, llvm::StringRef arg_name, llvm::StringRef arg,
             unsigned long& val) -> bool;

  // Overload in subclass to provide a better default value.
  auto getValueName() const -> llvm::StringRef override { return "ulong"; }

  void printOptionDiff(const Option& o, unsigned long v, OptVal Default,
                       size_t global_width) const;

  // An out-of-line virtual method to provide a 'home' for this class.
  void anchor() override;
};

//--------------------------------------------------

extern template class basic_parser<unsigned long long>;

template <>
class parser<unsigned long long> : public basic_parser<unsigned long long> {
 public:
  parser(Option& o) : basic_parser(o) {}

  // Return true on error.
  auto parse(Option& o, llvm::StringRef arg_name, llvm::StringRef arg,
             unsigned long long& val) -> bool;

  // Overload in subclass to provide a better default value.
  auto getValueName() const -> llvm::StringRef override { return "ulong"; }

  void printOptionDiff(const Option& o, unsigned long long v, OptVal Default,
                       size_t global_width) const;

  // An out-of-line virtual method to provide a 'home' for this class.
  void anchor() override;
};

//--------------------------------------------------

extern template class basic_parser<double>;

template <>
class parser<double> : public basic_parser<double> {
 public:
  parser(Option& o) : basic_parser(o) {}

  // Return true on error.
  auto parse(Option& o, llvm::StringRef arg_name, llvm::StringRef arg,
             double& val) -> bool;

  // Overload in subclass to provide a better default value.
  auto getValueName() const -> llvm::StringRef override { return "number"; }

  void printOptionDiff(const Option& o, double v, OptVal Default,
                       size_t global_width) const;

  // An out-of-line virtual method to provide a 'home' for this class.
  void anchor() override;
};

//--------------------------------------------------

extern template class basic_parser<float>;

template <>
class parser<float> : public basic_parser<float> {
 public:
  parser(Option& o) : basic_parser(o) {}

  // Return true on error.
  auto parse(Option& o, llvm::StringRef arg_name, llvm::StringRef arg,
             float& val) -> bool;

  // Overload in subclass to provide a better default value.
  auto getValueName() const -> llvm::StringRef override { return "number"; }

  void printOptionDiff(const Option& o, float v, OptVal Default,
                       size_t global_width) const;

  // An out-of-line virtual method to provide a 'home' for this class.
  void anchor() override;
};

//--------------------------------------------------

extern template class basic_parser<std::string>;

template <>
class parser<std::string> : public basic_parser<std::string> {
 public:
  parser(Option& o) : basic_parser(o) {}

  // Return true on error.
  auto parse(Option&, llvm::StringRef, llvm::StringRef arg, std::string& value)
      -> bool {
    value = arg.str();
    return false;
  }

  // Overload in subclass to provide a better default value.
  auto getValueName() const -> llvm::StringRef override { return "string"; }

  void printOptionDiff(const Option& o, llvm::StringRef v,
                       const OptVal& Default, size_t global_width) const;

  // An out-of-line virtual method to provide a 'home' for this class.
  void anchor() override;
};

//--------------------------------------------------

extern template class basic_parser<char>;

template <>
class parser<char> : public basic_parser<char> {
 public:
  parser(Option& o) : basic_parser(o) {}

  // Return true on error.
  auto parse(Option&, llvm::StringRef, llvm::StringRef arg, char& value)
      -> bool {
    value = arg[0];
    return false;
  }

  // Overload in subclass to provide a better default value.
  auto getValueName() const -> llvm::StringRef override { return "char"; }

  void printOptionDiff(const Option& o, char v, OptVal Default,
                       size_t global_width) const;

  // An out-of-line virtual method to provide a 'home' for this class.
  void anchor() override;
};

//--------------------------------------------------
// This collection of wrappers is the intermediary between class opt and class
// parser to handle all the template nastiness.

// This overloaded function is selected by the generic parser.
template <class ParserClass, class DT>
void printOptionDiff(const Option& O, const generic_parser_base& P, const DT& V,
                     const OptionValue<DT>& Default, size_t GlobalWidth) {
  OptionValue<DT> OV = V;
  P.printOptionDiff(O, OV, Default, GlobalWidth);
}

// This is instantiated for basic parsers when the parsed value has a different
// type than the option value. e.g. HelpPrinter.
template <class ParserDT, class ValDT>
struct OptionDiffPrinter {
  void print(const Option& O, const parser<ParserDT>& P, const ValDT& /*V*/,
             const OptionValue<ValDT>& /*Default*/, size_t GlobalWidth) {
    P.printOptionNoValue(O, GlobalWidth);
  }
};

// This is instantiated for basic parsers when the parsed value has the same
// type as the option value.
template <class DT>
struct OptionDiffPrinter<DT, DT> {
  void print(const Option& O, const parser<DT>& P, const DT& V,
             const OptionValue<DT>& Default, size_t GlobalWidth) {
    P.printOptionDiff(O, V, Default, GlobalWidth);
  }
};

// This overloaded function is selected by the basic parser, which may parse a
// different type than the option type.
template <class ParserClass, class ValDT>
void printOptionDiff(
    const Option& O,
    const basic_parser<typename ParserClass::parser_data_type>& P,
    const ValDT& V, const OptionValue<ValDT>& Default, size_t GlobalWidth) {
  OptionDiffPrinter<typename ParserClass::parser_data_type, ValDT> printer;
  printer.print(O, static_cast<const ParserClass&>(P), V, Default, GlobalWidth);
}

}  // namespace Commandline

#endif  // COMMANDLINE_PARSER_H