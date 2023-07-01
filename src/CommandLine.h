#ifndef COMMANDLINE_COMMANDLINE_H
#define COMMANDLINE_COMMANDLINE_H

#include <cassert>
#include <climits>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <string>
#include <type_traits>
#include <vector>

#include "ManagedStatic.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Twine.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/StringSaver.h"
#include "llvm/Support/raw_ostream.h"

namespace Commandline {

class Option;

//===----------------------------------------------------------------------===//
// Flags permitted to be passed to command line arguments
//

enum NumOccurrencesFlag {  // Flags for the number of occurrences allowed
  Optional = 0x00,         // Zero or One occurrence
  ZeroOrMore = 0x01,       // Zero or more occurrences allowed
  Required = 0x02,         // One occurrence required
  OneOrMore = 0x03,        // One or more occurrences required

  // Indicates that this option is fed anything that follows the last positional
  // argument required by the application (it is an error if there are zero
  // positional arguments, and a ConsumeAfter option is used).
  // Thus, for example, all arguments to LLI are processed until a filename is
  // found.  Once a filename is found, all of the succeeding arguments are
  // passed, unprocessed, to the ConsumeAfter option.
  //
  ConsumeAfter = 0x04
};

enum ValueExpected {  // Is a value required for the option?
  // zero reserved for the unspecified value
  ValueOptional = 0x01,   // The value can appear... or not
  ValueRequired = 0x02,   // The value is required to appear!
  ValueDisallowed = 0x03  // A value may not be specified (for flags)
};

enum OptionHidden {    // Control whether -help shows this option
  NotHidden = 0x00,    // Option included in -help & -help-hidden
  Hidden = 0x01,       // -help doesn't, but -help-hidden does
  ReallyHidden = 0x02  // Neither -help nor -help-hidden show this arg
};

// This controls special features that the option might have that cause it to be
// parsed differently...
//
// Prefix - This option allows arguments that are otherwise unrecognized to be
// matched by options that are a prefix of the actual value.  This is useful for
// cases like a linker, where options are typically of the form '-lfoo' or
// '-L../../include' where -l or -L are the actual flags.  When prefix is
// enabled, and used, the value for the flag comes from the suffix of the
// argument.
//
// AlwaysPrefix - Only allow the behavior enabled by the Prefix flag and reject
// the Option=Value form.
//

enum FormattingFlags {
  NormalFormatting = 0x00,  // Nothing special
  Positional = 0x01,        // Is a positional argument, no '-' required
  Prefix = 0x02,            // Can this option directly prefix its value?
  AlwaysPrefix = 0x03       // Can this option only directly prefix its value?
};

enum MiscFlags {          // Miscellaneous flags to adjust argument
  CommaSeparated = 0x01,  // Should this commandline::list split between commas?
  PositionalEatsArgs =
      0x02,     // Should this positional commandline::list eat -args?
  Sink = 0x04,  // Should this commandline::list eat all unknown options?

  // Can this option group with other options?
  // If this is enabled, multiple letter options are allowed to bunch together
  // with only a single hyphen for the whole group.  This allows emulation
  // of the behavior that ls uses for example: ls -la === ls -l -a
  Grouping = 0x08,

  // Default option
  DefaultOption = 0x10
};

//===----------------------------------------------------------------------===//
//
class OptionCategory {
 private:
  llvm::StringRef const Name;
  llvm::StringRef const Description;

  void registerCategory();

 public:
  OptionCategory(llvm::StringRef const name,
                 llvm::StringRef const description = "")
      : Name(name), Description(description) {
    registerCategory();
  }

  auto getName() const -> llvm::StringRef { return Name; }
  auto getDescription() const -> llvm::StringRef { return Description; }
};

// The general Option Category (used as default category).
auto getGeneralCategory() -> OptionCategory&;

//===----------------------------------------------------------------------===//
//
class SubCommand {
 private:
  llvm::StringRef Name;
  llvm::StringRef Description;

 protected:
  void registerSubCommand();
  void unregisterSubCommand();

 public:
  SubCommand(llvm::StringRef name, llvm::StringRef description = "")
      : Name(name), Description(description) {
    registerSubCommand();
  }
  SubCommand() = default;

  // Get the special subcommand representing no subcommand.
  static auto getTopLevel() -> SubCommand&;

  // Get the special subcommand that can be used to put an option into all
  // subcommands.
  static auto getAll() -> SubCommand&;

  void reset();

  explicit operator bool() const;

  auto getName() const -> llvm::StringRef { return Name; }
  auto getDescription() const -> llvm::StringRef { return Description; }

  llvm::SmallVector<Option*, 4> PositionalOpts;
  llvm::SmallVector<Option*, 4> SinkOpts;
  llvm::StringMap<Option*> OptionsMap;

  Option* ConsumeAfterOpt = nullptr;  // The ConsumeAfter option if it exists.
};

// A special subcommand representing no subcommand
extern ManagedStatic<SubCommand> top_level_sub_command;

// A special subcommand that can be used to put an option into all subcommands.
extern ManagedStatic<SubCommand> all_sub_commands;

//===----------------------------------------------------------------------===//
//
class Option {
  friend class alias;

  // Overriden by subclasses to handle the value passed into an argument. Should
  // return true if there was an error processing the argument and the program
  // should exit.
  //
  virtual auto handleOccurrence(unsigned pos, llvm::StringRef arg_name,
                                llvm::StringRef arg) -> bool = 0;

  virtual auto getValueExpectedFlagDefault() const -> enum ValueExpected {
    return ValueOptional;
  }

  // Out of line virtual function to provide home for the class.
  virtual void anchor();

  uint16_t NumOccurrences;  // The number of times specified
  // Occurrences, HiddenFlag, and Formatting are all enum types but to avoid
  // problems with signed enums in bitfields.
  uint16_t Occurrences : 3;  // enum NumOccurrencesFlag
  // not using the enum type for 'Value' because zero is an implementation
  // detail representing the non-value
  uint16_t Value : 2;
  uint16_t HiddenFlag : 2;  // enum OptionHidden
  uint16_t Formatting : 2;  // enum FormattingFlags
  uint16_t Misc : 5;
  uint16_t FullyInitialized : 1;  // Has addArgument been called?
  uint16_t Position;              // Position of last occurrence of the option
  uint16_t AdditionalVals;        // Greater than 0 for multi-valued option.

 public:
  llvm::StringRef ArgStr;   // The argument string itself (ex: "help", "o")
  llvm::StringRef HelpStr;  // The descriptive text message for -help
  llvm::StringRef
      ValueStr;  // String describing what the value of this option is
  llvm::SmallVector<OptionCategory*, 1>
      Categories;  // The Categories this option belongs to
  llvm::SmallPtrSet<SubCommand*, 1>
      Subs;  // The subcommands this option belongs to.

  inline auto getNumOccurrencesFlag() const -> enum NumOccurrencesFlag {
    return static_cast<enum NumOccurrencesFlag>(Occurrences);
  }

  inline auto getValueExpectedFlag() const -> enum ValueExpected {
    return Value ? (static_cast<enum ValueExpected>(Value))
                 : getValueExpectedFlagDefault();
  }

  inline auto getOptionHiddenFlag() const -> enum OptionHidden {
    return static_cast<enum OptionHidden>(HiddenFlag);
  }

  inline auto getFormattingFlag() const -> enum FormattingFlags {
    return static_cast<enum FormattingFlags>(Formatting);
  }

  inline auto getMiscFlags() const -> unsigned { return Misc; }
  inline auto getPosition() const -> unsigned { return Position; }
  inline auto getNumAdditionalVals() const -> unsigned {
    return AdditionalVals;
  }

  // Return true if the argstr != ""
  auto hasArgStr() const -> bool { return !ArgStr.empty(); }
  auto isPositional() const -> bool {
    return getFormattingFlag() == Commandline::Positional;
  }
  auto isSink() const -> bool { return getMiscFlags() & Commandline::Sink; }
  auto isDefaultOption() const -> bool {
    return getMiscFlags() & Commandline::DefaultOption;
  }

  auto isConsumeAfter() const -> bool {
    return getNumOccurrencesFlag() == Commandline::ConsumeAfter;
  }

  auto isInAllSubCommands() const -> bool {
    return Subs.contains(&SubCommand::getAll());
  }

  //-------------------------------------------------------------------------===
  // Accessor functions set by OptionModifiers
  //
  void setArgStr(llvm::StringRef s);
  void setDescription(llvm::StringRef s) { HelpStr = s; }
  void setValueStr(llvm::StringRef s) { ValueStr = s; }
  void setNumOccurrencesFlag(enum NumOccurrencesFlag val) { Occurrences = val; }
  void setValueExpectedFlag(enum ValueExpected val) { Value = val; }
  void setHiddenFlag(enum OptionHidden val) { HiddenFlag = val; }
  void setFormattingFlag(enum FormattingFlags v) { Formatting = v; }
  void setMiscFlag(enum MiscFlags m) { Misc |= m; }
  void setPosition(unsigned pos) { Position = pos; }
  void addCategory(OptionCategory& c);
  void addSubCommand(SubCommand& s) { Subs.insert(&s); }

 protected:
  explicit Option(enum NumOccurrencesFlag occurrences_flag,
                  enum OptionHidden hidden)
      : NumOccurrences(0),
        Occurrences(occurrences_flag),
        Value(0),
        HiddenFlag(hidden),
        Formatting(NormalFormatting),
        Misc(0),
        FullyInitialized(false),
        Position(0),
        AdditionalVals(0) {
    Categories.push_back(&getGeneralCategory());
  }

  inline void setNumAdditionalVals(unsigned n) { AdditionalVals = n; }

 public:
  virtual ~Option() = default;

  // Register this argument with the Commandline system.
  //
  void addArgument();

  /// Unregisters this option from the Commandline system.
  ///
  /// This option must have been the last option registered.
  /// For testing purposes only.
  void removeArgument();

  // Return the width of the option tag for printing...
  virtual auto getOptionWidth() const -> size_t = 0;

  // Print out information about this option. The to-be-maintained width is
  // specified.
  //
  virtual void printOptionInfo(size_t global_width) const = 0;

  virtual void printOptionValue(size_t global_width, bool force) const = 0;

  virtual void setDefault() = 0;

  // Prints the help string for an option.
  //
  // This maintains the Indent for multi-line descriptions.
  // FirstLineIndentedBy is the count of chars of the first line
  //      i.e. the one containing the --<option name>.
  static void printHelpStr(llvm::StringRef help_str, size_t indent,
                           size_t first_line_indented_by);

  // Prints the help string for an enum value.
  //
  // This maintains the Indent for multi-line descriptions.
  // FirstLineIndentedBy is the count of chars of the first line
  //      i.e. the one containing the =<value>.
  static void printEnumValHelpStr(llvm::StringRef help_str, size_t indent,
                                  size_t first_line_indented_by);

  virtual void getExtraOptionNames(llvm::SmallVectorImpl<llvm::StringRef>&) {}

  // Wrapper around handleOccurrence that enforces Flags.
  //
  virtual auto addOccurrence(unsigned pos, llvm::StringRef arg_name,
                             llvm::StringRef value, bool multi_arg = false)
      -> bool;

  // Prints option name followed by message.  Always returns true.
  auto error(const llvm::Twine& message,
             llvm::StringRef arg_name = llvm::StringRef(),
             llvm::raw_ostream& errs = llvm::errs()) -> bool;
  auto error(const llvm::Twine& message, llvm::raw_ostream& errs) -> bool {
    return error(message, llvm::StringRef(), errs);
  }

  inline auto getNumOccurrences() const -> int { return NumOccurrences; }
  void reset();
};

//===----------------------------------------------------------------------===//
// Command line option modifiers that can be used to modify the behavior of
// command line option parsers...
//

// Modifier to set the description shown in the -help output...
struct desc {
  llvm::StringRef Desc;

  desc(llvm::StringRef Str) : Desc(Str) {}

  void apply(Option& o) const { o.setDescription(Desc); }
};

// Modifier to set the value description shown in the -help output...
struct value_desc {
  llvm::StringRef Desc;

  value_desc(llvm::StringRef str) : Desc(str) {}

  void apply(Option& o) const { o.setValueStr(Desc); }
};

// Specify a default (initial) value for the command line argument, if the
// default constructor for the argument type does not give you what you want.
// This is only valid on "opt" arguments, not on "list" arguments.
template <class Ty>
struct Initializer {
  const Ty& Init;
  Initializer(const Ty& Val) : Init(Val) {}

  template <class Opt>
  void apply(Opt& O) const {
    O.setInitialValue(Init);
  }
};

template <class Ty>
struct ListInitializer {
  llvm::ArrayRef<Ty> Inits;
  ListInitializer(llvm::ArrayRef<Ty> vals) : Inits(vals) {}

  template <class Opt>
  void apply(Opt& o) const {
    o.setInitialValues(Inits);
  }
};

template <class Ty>
auto init(const Ty& val) -> Initializer<Ty> {
  return Initializer<Ty>(val);
}

template <class Ty>
auto list_init(llvm::ArrayRef<Ty> vals) -> ListInitializer<Ty> {
  return ListInitializer<Ty>(vals);
}

// Allow the user to specify which external variable they want to store the
// results of the command line argument processing into, if they don't want to
// store it in the option itself.
template <class Ty>
struct LocationClass {
  Ty& Loc;

  LocationClass(Ty& l) : Loc(l) {}

  template <class Opt>
  void apply(Opt& o) const {
    o.setLocation(o, Loc);
  }
};

template <class Ty>
auto location(Ty& l) -> LocationClass<Ty> {
  return LocationClass<Ty>(l);
}

// Specify the Option category for the command line argument to belong to.
struct cat {
  OptionCategory& Category;

  cat(OptionCategory& c) : Category(c) {}

  template <class Opt>
  void apply(Opt& O) const {
    O.addCategory(Category);
  }
};

// Specify the subcommand that this option belongs to.
struct sub {
  SubCommand& Sub;

  sub(SubCommand& s) : Sub(s) {}

  template <class Opt>
  void apply(Opt& O) const {
    O.addSubCommand(Sub);
  }
};

// Specify a callback function to be called when an option is seen.
// Can be used to set other options automatically.
template <typename R, typename Ty>
struct cb {
  std::function<R(Ty)> CB;

  cb(std::function<R(Ty)> CB) : CB(CB) {}

  template <typename Opt>
  void apply(Opt& O) const {
    O.setCallback(CB);
  }
};

namespace detail {
template <typename F>
struct callback_traits : public callback_traits<decltype(&F::operator())> {};

template <typename R, typename C, typename... Args>
struct callback_traits<R (C::*)(Args...) const> {
  using result_type = R;
  using arg_type = std::tuple_element_t<0, std::tuple<Args...>>;
  static_assert(sizeof...(Args) == 1,
                "callback function must have one and only one parameter");
  static_assert(std::is_same_v<result_type, void>,
                "callback return type must be void");
  static_assert(std::is_lvalue_reference_v<arg_type> &&
                    std::is_const_v<std::remove_reference_t<arg_type>>,
                "callback arg_type must be a const lvalue reference");
};
}  // namespace detail

template <typename F>
auto callback(F CB) -> cb<typename detail::callback_traits<F>::result_type,
                          typename detail::callback_traits<F>::arg_type> {
  using result_type = typename detail::callback_traits<F>::result_type;
  using arg_type = typename detail::callback_traits<F>::arg_type;
  return cb<result_type, arg_type>(CB);
}

//===----------------------------------------------------------------------===//

// Support value comparison outside the template.
struct GenericOptionValue {
  virtual bool compare(const GenericOptionValue& V) const = 0;

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
template <class DataType, bool isClass>
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
    if (!vc.hasValue())
      return false;
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
struct OptionValue<Commandline::boolOrDefault> final
    : OptionValueCopy<Commandline::boolOrDefault> {
  using WrapperType = Commandline::boolOrDefault;

  OptionValue() = default;

  OptionValue(const Commandline::boolOrDefault& v) { this->setValue(v); }

  auto operator=(const Commandline::boolOrDefault& v)
      -> OptionValue<Commandline::boolOrDefault>& {
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
  Commandline::OptionEnumValue { #ENUMVAL, int(ENUMVAL), DESC }
#define clEnumValN(ENUMVAL, FLAGNAME, DESC) \
  Commandline::OptionEnumValue { FLAGNAME, int(ENUMVAL), DESC }

// For custom data types, allow specifying a group of values together as the
// values that go into the mapping that the option handler uses.
//
class ValuesClass {
  // Use a vector instead of a map, because the lists should be short,
  // the overhead is less, and most importantly, it keeps them in the order
  // inserted so we can print our option out nicely.
  llvm::SmallVector<OptionEnumValue, 4> Values;

 public:
  ValuesClass(std::initializer_list<OptionEnumValue> Options)
      : Values(Options) {}

  template <class Opt>
  void apply(Opt& O) const {
    for (const auto& Value : Values)
      O.getParser().addLiteralOption(Value.Name, Value.Value,
                                     Value.Description);
  }
};

/// Helper to build a ValuesClass by forwarding a variable number of arguments
/// as an initializer list to the ValuesClass constructor.
template <typename... OptsTy>
ValuesClass values(OptsTy... Options) {
  return ValuesClass({Options...});
}

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

#endif  // COMMANDLINE_COMMANDLINE_H