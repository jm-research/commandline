#ifndef COMMANDLINE_SUBCOMMAND_H
#define COMMANDLINE_SUBCOMMAND_H

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringMap.h"

#include "ManagedStatic.h"

namespace Commandline {

class Option;

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

}  // namespace Commandline

#endif  // COMMANDLINE_SUBCOMMAND_H