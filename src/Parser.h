#ifndef COMMANDLEINE_PARSER_H
#define COMMANDLEINE_PARSER_H

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
    GenericOptionInfo(llvm::StringRef name, llvm::StringRef helpStr)
        : Name(name), HelpStr(helpStr) {}
    llvm::StringRef Name;
    llvm::StringRef HelpStr;
  };

 public:
  generic_parser_base(Option& O) : Owner(O) {}

  virtual ~generic_parser_base() = default;
  // Base class should have virtual-destructor

  // Virtual function implemented by generic subclass to indicate how many
  // entries are in Values.
  //
  virtual unsigned getNumOptions() const = 0;

  // Return option name N.
  virtual llvm::StringRef getOption(unsigned N) const = 0;

  // Return description N
  virtual llvm::StringRef getDescription(unsigned N) const = 0;

  // Return the width of the option tag for printing...
  virtual size_t getOptionWidth(const Option& O) const;

  virtual const GenericOptionValue& getOptionValue(unsigned N) const = 0;

  // Print out information about this option. The to-be-maintained width is
  // specified.
  //
  virtual void printOptionInfo(const Option& O, size_t GlobalWidth) const;

  void printGenericOptionDiff(const Option& O, const GenericOptionValue& V,
                              const GenericOptionValue& Default,
                              size_t GlobalWidth) const;

  // Print the value of an option and it's default.
  //
  // Template definition ensures that the option and default have the same
  // DataType (via the same AnyOptionValue).
  template <class AnyOptionValue>
  void printOptionDiff(const Option& O, const AnyOptionValue& V,
                       const AnyOptionValue& Default,
                       size_t GlobalWidth) const {
    printGenericOptionDiff(O, V, Default, GlobalWidth);
  }

  void initialize() {}

  void getExtraOptionNames(
      llvm::SmallVectorImpl<llvm::StringRef>& OptionNames) {
    // If there has been no argstr specified, that means that we need to add an
    // argument for every possible option.  This ensures that our options are
    // vectored to us.
    if (!Owner.hasArgStr())
      for (unsigned i = 0, e = getNumOptions(); i != e; ++i)
        OptionNames.push_back(getOption(i));
  }

  enum ValueExpected getValueExpectedFlagDefault() const {
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
    if (Owner.hasArgStr())
      return ValueRequired;
    else
      return ValueDisallowed;
  }

  // Return the option number corresponding to the specified
  // argument string.  If the option is not found, getNumOptions() is returned.
  //
  unsigned findOption(llvm::StringRef Name);

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
    OptionInfo(llvm::StringRef name, DataType v, llvm::StringRef helpStr)
        : GenericOptionInfo(name, helpStr), V(v) {}

    OptionValue<DataType> V;
  };
  llvm::SmallVector<OptionInfo, 8> Values;

 public:
  parser(Option& O) : generic_parser_base(O) {}

  using parser_data_type = DataType;

  // Implement virtual functions needed by generic_parser_base
  unsigned getNumOptions() const override { return unsigned(Values.size()); }
  llvm::StringRef getOption(unsigned N) const override {
    return Values[N].Name;
  }
  llvm::StringRef getDescription(unsigned N) const override {
    return Values[N].HelpStr;
  }

  // Return the value of option name N.
  const GenericOptionValue& getOptionValue(unsigned N) const override {
    return Values[N].V;
  }

  // Return true on error.
  bool parse(Option& O, llvm::StringRef ArgName, llvm::StringRef Arg,
             DataType& V) {
    llvm::StringRef ArgVal;
    if (Owner.hasArgStr())
      ArgVal = Arg;
    else
      ArgVal = ArgName;

    for (size_t i = 0, e = Values.size(); i != e; ++i)
      if (Values[i].Name == ArgVal) {
        V = Values[i].V.getValue();
        return false;
      }

    return O.error("Cannot find option named '" + ArgVal + "'!");
  }

  /// Add an entry to the mapping table.
  ///
  template <class DT>
  void addLiteralOption(llvm::StringRef Name, const DT& V,
                        llvm::StringRef HelpStr) {
    assert(findOption(Name) == Values.size() && "Option already exists!");
    OptionInfo X(Name, static_cast<DataType>(V), HelpStr);
    Values.push_back(X);
    AddLiteralOption(Owner, Name);
  }

  /// Remove the specified option.
  ///
  void removeLiteralOption(llvm::StringRef Name) {
    unsigned N = findOption(Name);
    assert(N != Values.size() && "Option not found!");
    Values.erase(Values.begin() + N);
  }
};

//--------------------------------------------------
// Super class of parsers to provide boilerplate code
//
class basic_parser_impl {  // non-template implementation of basic_parser<t>
 public:
  basic_parser_impl(Option&) {}

  virtual ~basic_parser_impl() = default;

  enum ValueExpected getValueExpectedFlagDefault() const {
    return ValueRequired;
  }

  void getExtraOptionNames(llvm::SmallVectorImpl<llvm::StringRef>&) {}

  void initialize() {}

  // Return the width of the option tag for printing...
  size_t getOptionWidth(const Option& O) const;

  // Print out information about this option. The to-be-maintained width is
  // specified.
  //
  void printOptionInfo(const Option& O, size_t GlobalWidth) const;

  // Print a placeholder for options that don't yet support printOptionDiff().
  void printOptionNoValue(const Option& O, size_t GlobalWidth) const;

  // Overload in subclass to provide a better default value.
  virtual llvm::StringRef getValueName() const { return "value"; }

  // An out-of-line virtual method to provide a 'home' for this class.
  virtual void anchor();

 protected:
  // A helper for basic_parser::printOptionDiff.
  void printOptionName(const Option& O, size_t GlobalWidth) const;
};

// The real basic parser is just a template wrapper that provides a typedef for
// the provided data type.
//
template <class DataType>
class basic_parser : public basic_parser_impl {
 public:
  using parser_data_type = DataType;
  using OptVal = OptionValue<DataType>;

  basic_parser(Option& O) : basic_parser_impl(O) {}
};

//--------------------------------------------------

extern template class basic_parser<bool>;

template <>
class parser<bool> : public basic_parser<bool> {
 public:
  parser(Option& O) : basic_parser(O) {}

  // Return true on error.
  bool parse(Option& O, llvm::StringRef ArgName, llvm::StringRef Arg,
             bool& Val);

  void initialize() {}

  enum ValueExpected getValueExpectedFlagDefault() const {
    return ValueOptional;
  }

  // Do not print =<value> at all.
  llvm::StringRef getValueName() const override { return llvm::StringRef(); }

  void printOptionDiff(const Option& O, bool V, OptVal Default,
                       size_t GlobalWidth) const;

  // An out-of-line virtual method to provide a 'home' for this class.
  void anchor() override;
};

//--------------------------------------------------

extern template class basic_parser<boolOrDefault>;

template <>
class parser<boolOrDefault> : public basic_parser<boolOrDefault> {
 public:
  parser(Option& O) : basic_parser(O) {}

  // Return true on error.
  bool parse(Option& O, llvm::StringRef ArgName, llvm::StringRef Arg,
             boolOrDefault& Val);

  enum ValueExpected getValueExpectedFlagDefault() const {
    return ValueOptional;
  }

  // Do not print =<value> at all.
  llvm::StringRef getValueName() const override { return llvm::StringRef(); }

  void printOptionDiff(const Option& O, boolOrDefault V, OptVal Default,
                       size_t GlobalWidth) const;

  // An out-of-line virtual method to provide a 'home' for this class.
  void anchor() override;
};

//--------------------------------------------------

extern template class basic_parser<int>;

template <>
class parser<int> : public basic_parser<int> {
 public:
  parser(Option& O) : basic_parser(O) {}

  // Return true on error.
  bool parse(Option& O, llvm::StringRef ArgName, llvm::StringRef Arg, int& Val);

  // Overload in subclass to provide a better default value.
  llvm::StringRef getValueName() const override { return "int"; }

  void printOptionDiff(const Option& O, int V, OptVal Default,
                       size_t GlobalWidth) const;

  // An out-of-line virtual method to provide a 'home' for this class.
  void anchor() override;
};

//--------------------------------------------------

extern template class basic_parser<long>;

template <>
class parser<long> final : public basic_parser<long> {
 public:
  parser(Option& O) : basic_parser(O) {}

  // Return true on error.
  bool parse(Option& O, llvm::StringRef ArgName, llvm::StringRef Arg,
             long& Val);

  // Overload in subclass to provide a better default value.
  llvm::StringRef getValueName() const override { return "long"; }

  void printOptionDiff(const Option& O, long V, OptVal Default,
                       size_t GlobalWidth) const;

  // An out-of-line virtual method to provide a 'home' for this class.
  void anchor() override;
};

//--------------------------------------------------

extern template class basic_parser<long long>;

template <>
class parser<long long> : public basic_parser<long long> {
 public:
  parser(Option& O) : basic_parser(O) {}

  // Return true on error.
  bool parse(Option& O, llvm::StringRef ArgName, llvm::StringRef Arg,
             long long& Val);

  // Overload in subclass to provide a better default value.
  llvm::StringRef getValueName() const override { return "long"; }

  void printOptionDiff(const Option& O, long long V, OptVal Default,
                       size_t GlobalWidth) const;

  // An out-of-line virtual method to provide a 'home' for this class.
  void anchor() override;
};

//--------------------------------------------------

extern template class basic_parser<unsigned>;

template <>
class parser<unsigned> : public basic_parser<unsigned> {
 public:
  parser(Option& O) : basic_parser(O) {}

  // Return true on error.
  bool parse(Option& O, llvm::StringRef ArgName, llvm::StringRef Arg,
             unsigned& Val);

  // Overload in subclass to provide a better default value.
  llvm::StringRef getValueName() const override { return "uint"; }

  void printOptionDiff(const Option& O, unsigned V, OptVal Default,
                       size_t GlobalWidth) const;

  // An out-of-line virtual method to provide a 'home' for this class.
  void anchor() override;
};

//--------------------------------------------------

extern template class basic_parser<unsigned long>;

template <>
class parser<unsigned long> final : public basic_parser<unsigned long> {
 public:
  parser(Option& O) : basic_parser(O) {}

  // Return true on error.
  bool parse(Option& O, llvm::StringRef ArgName, llvm::StringRef Arg,
             unsigned long& Val);

  // Overload in subclass to provide a better default value.
  llvm::StringRef getValueName() const override { return "ulong"; }

  void printOptionDiff(const Option& O, unsigned long V, OptVal Default,
                       size_t GlobalWidth) const;

  // An out-of-line virtual method to provide a 'home' for this class.
  void anchor() override;
};

//--------------------------------------------------

extern template class basic_parser<unsigned long long>;

template <>
class parser<unsigned long long> : public basic_parser<unsigned long long> {
 public:
  parser(Option& O) : basic_parser(O) {}

  // Return true on error.
  bool parse(Option& O, llvm::StringRef ArgName, llvm::StringRef Arg,
             unsigned long long& Val);

  // Overload in subclass to provide a better default value.
  llvm::StringRef getValueName() const override { return "ulong"; }

  void printOptionDiff(const Option& O, unsigned long long V, OptVal Default,
                       size_t GlobalWidth) const;

  // An out-of-line virtual method to provide a 'home' for this class.
  void anchor() override;
};

//--------------------------------------------------

extern template class basic_parser<double>;

template <>
class parser<double> : public basic_parser<double> {
 public:
  parser(Option& O) : basic_parser(O) {}

  // Return true on error.
  bool parse(Option& O, llvm::StringRef ArgName, llvm::StringRef Arg,
             double& Val);

  // Overload in subclass to provide a better default value.
  llvm::StringRef getValueName() const override { return "number"; }

  void printOptionDiff(const Option& O, double V, OptVal Default,
                       size_t GlobalWidth) const;

  // An out-of-line virtual method to provide a 'home' for this class.
  void anchor() override;
};

//--------------------------------------------------

extern template class basic_parser<float>;

template <>
class parser<float> : public basic_parser<float> {
 public:
  parser(Option& O) : basic_parser(O) {}

  // Return true on error.
  bool parse(Option& O, llvm::StringRef ArgName, llvm::StringRef Arg,
             float& Val);

  // Overload in subclass to provide a better default value.
  llvm::StringRef getValueName() const override { return "number"; }

  void printOptionDiff(const Option& O, float V, OptVal Default,
                       size_t GlobalWidth) const;

  // An out-of-line virtual method to provide a 'home' for this class.
  void anchor() override;
};

//--------------------------------------------------

extern template class basic_parser<std::string>;

template <>
class parser<std::string> : public basic_parser<std::string> {
 public:
  parser(Option& O) : basic_parser(O) {}

  // Return true on error.
  bool parse(Option&, llvm::StringRef, llvm::StringRef Arg,
             std::string& Value) {
    Value = Arg.str();
    return false;
  }

  // Overload in subclass to provide a better default value.
  llvm::StringRef getValueName() const override { return "string"; }

  void printOptionDiff(const Option& O, llvm::StringRef V,
                       const OptVal& Default, size_t GlobalWidth) const;

  // An out-of-line virtual method to provide a 'home' for this class.
  void anchor() override;
};

//--------------------------------------------------

extern template class basic_parser<char>;

template <>
class parser<char> : public basic_parser<char> {
 public:
  parser(Option& O) : basic_parser(O) {}

  // Return true on error.
  bool parse(Option&, llvm::StringRef, llvm::StringRef Arg, char& Value) {
    Value = Arg[0];
    return false;
  }

  // Overload in subclass to provide a better default value.
  llvm::StringRef getValueName() const override { return "char"; }

  void printOptionDiff(const Option& O, char V, OptVal Default,
                       size_t GlobalWidth) const;

  // An out-of-line virtual method to provide a 'home' for this class.
  void anchor() override;
};

}  // namespace Commandline

#endif  // COMMANDLEINE_PARSER_H