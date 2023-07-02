#ifndef COMMANDLINE_OPTIONCATEGORY_H
#define COMMANDLINE_OPTIONCATEGORY_H

#include "llvm/ADT/StringRef.h"

namespace Commandline {

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

}  // namespace Commandline

#endif  // COMMANDLINE_OPTIONCATEGORY_H