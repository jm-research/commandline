#ifndef COMMANDLINE_OPTION_H
#define COMMANDLINE_OPTION_H

#include "OptionCategory.h"
#include "OptionEnum.h"
#include "SubCommand.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

namespace Commandline {

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

}  // namespace Commandline

#endif  // COMMANDLINE_OPTION_H