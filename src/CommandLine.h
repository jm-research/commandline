#ifndef COMMANDLINE_COMMANDLINE_H
#define COMMANDLINE_COMMANDLINE_H

#include "Option.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Twine.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/StringSaver.h"
#include "llvm/Support/VirtualFileSystem.h"
#include "llvm/Support/raw_ostream.h"

namespace Commandline {

//===----------------------------------------------------------------------===//
// Command line option processing entry point.
//
// Returns true on success. Otherwise, this will print the error message to
// stderr and exit if \p Errs is not set (nullptr by default), or print the
// error message to \p Errs and return false if \p Errs is provided.
//
// If EnvVar is not nullptr, command-line options are also parsed from the
// environment variable named by EnvVar.  Precedence is given to occurrences
// from argv.  This precedence is currently implemented by parsing argv after
// the environment variable, so it is only implemented correctly for options
// that give precedence to later occurrences.  If your program supports options
// that give precedence to earlier occurrences, you will need to extend this
// function to support it correctly.
bool ParseCommandLineOptions(int argc, const char* const* argv,
                             llvm::StringRef Overview = "",
                             llvm::raw_ostream* Errs = nullptr,
                             const char* EnvVar = nullptr,
                             bool LongOptionsUseDoubleDash = false);

// Function pointer type for printing version information.
using VersionPrinterTy = std::function<void(llvm::raw_ostream&)>;

///===---------------------------------------------------------------------===//
/// Override the default (LLVM specific) version printer used to print out the
/// version when --version is given on the command line. This allows other
/// systems using the CommandLine utilities to print their own version string.
void SetVersionPrinter(VersionPrinterTy func);

///===---------------------------------------------------------------------===//
/// Add an extra printer to use in addition to the default one. This can be
/// called multiple times, and each time it adds a new function to the list
/// which will be called after the basic LLVM version printing is complete.
/// Each can then add additional information specific to the tool.
void AddExtraVersionPrinter(VersionPrinterTy func);

// Print option values.
// With -print-options print the difference between option values and defaults.
// With -print-all-options print all option values.
// (Currently not perfect, but best-effort.)
void PrintOptionValues();

// Forward declaration - AddLiteralOption needs to be up here to make gcc happy.
class Option;

/// Adds a new option for parsing and provides the option it refers to.
///
/// \param O pointer to the option
/// \param Name the string name for the option to handle during parsing
///
/// Literal options are used by some parsers to register special option values.
/// This is how the PassNameParser registers pass names for opt.
void AddLiteralOption(Option& O, llvm::StringRef Name);

// Provide additional help at the end of the normal help output. All occurrences
// of cl::extrahelp will be accumulated and printed to stderr at the end of the
// regular help, just before exit is called.
struct extrahelp {
  llvm::StringRef morehelp;

  explicit extrahelp(llvm::StringRef help);
};

void PrintVersionMessage();

/// This function just prints the help message, exactly the same way as if the
/// -help or -help-hidden option had been given on the command line.
///
/// \param Hidden if true will print hidden options
/// \param Categorized if true print options in categories
void PrintHelpMessage(bool Hidden = false, bool Categorized = false);

//===----------------------------------------------------------------------===//
// Public interface for accessing registered options.
//

/// Use this to get a StringMap to all registered named options
/// (e.g. -help).
///
/// \return A reference to the StringMap used by the cl APIs to parse options.
///
/// Access to unnamed arguments (i.e. positional) are not provided because
/// it is expected that the client already has access to these.
///
/// Typical usage:
/// \code
/// main(int argc,char* argv[]) {
/// StringMap<llvm::cl::Option*> &opts = llvm::cl::getRegisteredOptions();
/// assert(opts.count("help") == 1)
/// opts["help"]->setDescription("Show alphabetical help information")
/// // More code
/// llvm::cl::ParseCommandLineOptions(argc,argv);
/// //More code
/// }
/// \endcode
///
/// This interface is useful for modifying options in libraries that are out of
/// the control of the client. The options should be modified before calling
/// llvm::cl::ParseCommandLineOptions().
///
/// Hopefully this API can be deprecated soon. Any situation where options need
/// to be modified by tools or libraries should be handled by sane APIs rather
/// than just handing around a global list.
llvm::StringMap<Option*>& getRegisteredOptions(
    SubCommand& Sub = SubCommand::getTopLevel());

/// Use this to get all registered SubCommands from the provided parser.
///
/// \return A range of all SubCommand pointers registered with the parser.
///
/// Typical usage:
/// \code
/// main(int argc, char* argv[]) {
///   llvm::cl::ParseCommandLineOptions(argc, argv);
///   for (auto* S : llvm::cl::getRegisteredSubcommands()) {
///     if (*S) {
///       std::cout << "Executing subcommand: " << S->getName() << std::endl;
///       // Execute some function based on the name...
///     }
///   }
/// }
/// \endcode
///
/// This interface is useful for defining subcommands in libraries and
/// the dispatch from a single point (like in the main function).
llvm::iterator_range<typename llvm::SmallPtrSet<SubCommand*, 4>::iterator>
getRegisteredSubcommands();

//===----------------------------------------------------------------------===//
// Standalone command line processing utilities.
//

/// Tokenizes a command line that can contain escapes and quotes.
//
/// The quoting rules match those used by GCC and other tools that use
/// libiberty's buildargv() or expandargv() utilities, and do not match bash.
/// They differ from buildargv() on treatment of backslashes that do not escape
/// a special character to make it possible to accept most Windows file paths.
///
/// \param [in] Source The string to be split on whitespace with quotes.
/// \param [in] Saver Delegates back to the caller for saving parsed strings.
/// \param [in] MarkEOLs true if tokenizing a response file and you want end of
/// lines and end of the response file to be marked with a nullptr string.
/// \param [out] NewArgv All parsed strings are appended to NewArgv.
void TokenizeGNUCommandLine(llvm::StringRef Source, llvm::StringSaver& Saver,
                            llvm::SmallVectorImpl<const char*>& NewArgv,
                            bool MarkEOLs = false);

/// Tokenizes a string of Windows command line arguments, which may contain
/// quotes and escaped quotes.
///
/// See MSDN docs for CommandLineToArgvW for information on the quoting rules.
/// http://msdn.microsoft.com/en-us/library/windows/desktop/17w5ykft(v=vs.85).aspx
///
/// For handling a full Windows command line including the executable name at
/// the start, see TokenizeWindowsCommandLineFull below.
///
/// \param [in] Source The string to be split on whitespace with quotes.
/// \param [in] Saver Delegates back to the caller for saving parsed strings.
/// \param [in] MarkEOLs true if tokenizing a response file and you want end of
/// lines and end of the response file to be marked with a nullptr string.
/// \param [out] NewArgv All parsed strings are appended to NewArgv.
void TokenizeWindowsCommandLine(llvm::StringRef Source,
                                llvm::StringSaver& Saver,
                                llvm::SmallVectorImpl<const char*>& NewArgv,
                                bool MarkEOLs = false);

/// Tokenizes a Windows command line while attempting to avoid copies. If no
/// quoting or escaping was used, this produces substrings of the original
/// string. If a token requires unquoting, it will be allocated with the
/// StringSaver.
void TokenizeWindowsCommandLineNoCopy(
    llvm::StringRef Source, llvm::StringSaver& Saver,
    llvm::SmallVectorImpl<llvm::StringRef>& NewArgv);

/// Tokenizes a Windows full command line, including command name at the start.
///
/// This uses the same syntax rules as TokenizeWindowsCommandLine for all but
/// the first token. But the first token is expected to be parsed as the
/// executable file name in the way CreateProcess would do it, rather than the
/// way the C library startup code would do it: CreateProcess does not consider
/// that \ is ever an escape character (because " is not a valid filename char,
/// hence there's never a need to escape it to be used literally).
///
/// Parameters are the same as for TokenizeWindowsCommandLine. In particular,
/// if you set MarkEOLs = true, then the first word of every line will be
/// parsed using the special rules for command names, making this function
/// suitable for parsing a file full of commands to execute.
void TokenizeWindowsCommandLineFull(llvm::StringRef Source,
                                    llvm::StringSaver& Saver,
                                    llvm::SmallVectorImpl<const char*>& NewArgv,
                                    bool MarkEOLs = false);

/// String tokenization function type.  Should be compatible with either
/// Windows or Unix command line tokenizers.
using TokenizerCallback = void (*)(llvm::StringRef Source,
                                   llvm::StringSaver& Saver,
                                   llvm::SmallVectorImpl<const char*>& NewArgv,
                                   bool MarkEOLs);

/// Tokenizes content of configuration file.
///
/// \param [in] Source The string representing content of config file.
/// \param [in] Saver Delegates back to the caller for saving parsed strings.
/// \param [out] NewArgv All parsed strings are appended to NewArgv.
/// \param [in] MarkEOLs Added for compatibility with TokenizerCallback.
///
/// It works like TokenizeGNUCommandLine with ability to skip comment lines.
///
void tokenizeConfigFile(llvm::StringRef Source, llvm::StringSaver& Saver,
                        llvm::SmallVectorImpl<const char*>& NewArgv,
                        bool MarkEOLs = false);

/// Contains options that control response file expansion.
class ExpansionContext {
  /// Provides persistent storage for parsed strings.
  llvm::StringSaver Saver;

  /// Tokenization strategy. Typically Unix or Windows.
  TokenizerCallback Tokenizer;

  /// File system used for all file access when running the expansion.
  llvm::vfs::FileSystem* FS;

  /// Path used to resolve relative rsp files. If empty, the file system
  /// current directory is used instead.
  llvm::StringRef CurrentDir;

  /// Directories used for search of config files.
  llvm::ArrayRef<llvm::StringRef> SearchDirs;

  /// True if names of nested response files must be resolved relative to
  /// including file.
  bool RelativeNames = false;

  /// If true, mark end of lines and the end of the response file with nullptrs
  /// in the Argv vector.
  bool MarkEOLs = false;

  /// If true, body of config file is expanded.
  bool InConfigFile = false;

  llvm::Error expandResponseFile(llvm::StringRef FName,
                                 llvm::SmallVectorImpl<const char*>& NewArgv);

 public:
  ExpansionContext(llvm::BumpPtrAllocator& A, TokenizerCallback T);

  ExpansionContext& setMarkEOLs(bool X) {
    MarkEOLs = X;
    return *this;
  }

  ExpansionContext& setRelativeNames(bool X) {
    RelativeNames = X;
    return *this;
  }

  ExpansionContext& setCurrentDir(llvm::StringRef X) {
    CurrentDir = X;
    return *this;
  }

  ExpansionContext& setSearchDirs(llvm::ArrayRef<llvm::StringRef> X) {
    SearchDirs = X;
    return *this;
  }

  ExpansionContext& setVFS(llvm::vfs::FileSystem* X) {
    FS = X;
    return *this;
  }

  /// Looks for the specified configuration file.
  ///
  /// \param[in]  FileName Name of the file to search for.
  /// \param[out] FilePath File absolute path, if it was found.
  /// \return True if file was found.
  ///
  /// If the specified file name contains a directory separator, it is searched
  /// for by its absolute path. Otherwise looks for file sequentially in
  /// directories specified by SearchDirs field.
  bool findConfigFile(llvm::StringRef FileName,
                      llvm::SmallVectorImpl<char>& FilePath);

  /// Reads command line options from the given configuration file.
  ///
  /// \param [in] CfgFile Path to configuration file.
  /// \param [out] Argv Array to which the read options are added.
  /// \return true if the file was successfully read.
  ///
  /// It reads content of the specified file, tokenizes it and expands "@file"
  /// commands resolving file names in them relative to the directory where
  /// CfgFilename resides. It also expands "<CFGDIR>" to the base path of the
  /// current config file.
  llvm::Error readConfigFile(llvm::StringRef CfgFile,
                             llvm::SmallVectorImpl<const char*>& Argv);

  /// Expands constructs "@file" in the provided array of arguments recursively.
  llvm::Error expandResponseFiles(llvm::SmallVectorImpl<const char*>& Argv);
};

/// A convenience helper which concatenates the options specified by the
/// environment variable EnvVar and command line options, then expands
/// response files recursively.
/// \return true if all @files were expanded successfully or there were none.
bool expandResponseFiles(int Argc, const char* const* Argv, const char* EnvVar,
                         llvm::SmallVectorImpl<const char*>& NewArgv);

/// A convenience helper which supports the typical use case of expansion
/// function call.
bool ExpandResponseFiles(llvm::StringSaver& Saver, TokenizerCallback Tokenizer,
                         llvm::SmallVectorImpl<const char*>& Argv);

/// A convenience helper which concatenates the options specified by the
/// environment variable EnvVar and command line options, then expands response
/// files recursively. The tokenizer is a predefined GNU or Windows one.
/// \return true if all @files were expanded successfully or there were none.
bool expandResponseFiles(int Argc, const char* const* Argv, const char* EnvVar,
                         llvm::StringSaver& Saver,
                         llvm::SmallVectorImpl<const char*>& NewArgv);

/// Mark all options not part of this category as cl::ReallyHidden.
///
/// \param Category the category of options to keep displaying
///
/// Some tools (like clang-format) like to be able to hide all options that are
/// not specific to the tool. This function allows a tool to specify a single
/// option category to display in the -help output.
void HideUnrelatedOptions(OptionCategory& Category,
                          SubCommand& Sub = SubCommand::getTopLevel());

/// Mark all options not part of the categories as cl::ReallyHidden.
///
/// \param Categories the categories of options to keep displaying.
///
/// Some tools (like clang-format) like to be able to hide all options that are
/// not specific to the tool. This function allows a tool to specify a single
/// option category to display in the -help output.
void HideUnrelatedOptions(llvm::ArrayRef<const OptionCategory*> Categories,
                          SubCommand& Sub = SubCommand::getTopLevel());

/// Reset all command line options to a state that looks as if they have
/// never appeared on the command line.  This is useful for being able to parse
/// a command line multiple times (especially useful for writing tests).
void ResetAllOptionOccurrences();

/// Reset the command line parser back to its initial state.  This
/// removes
/// all options, categories, and subcommands and returns the parser to a state
/// where no options are supported.
void ResetCommandLineParser();

/// Parses `Arg` into the option handler `Handler`.
bool ProvidePositionalOption(Option* Handler, llvm::StringRef Arg, int i);

}  // namespace Commandline

#endif  // COMMANDLINE_COMMANDLINE_H