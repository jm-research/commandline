#ifndef COMMANDLINE_OPTIONENUM_H
#define COMMANDLINE_OPTIONENUM_H

namespace Commandline {

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

}  // namespace Commandline

#endif  // COMMANDLINE_OPTIONENUM_H