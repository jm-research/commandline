#ifndef COMMANDLINE_ALIAS_H
#define COMMANDLINE_ALIAS_H

#include "Option.h"
#include "llvm/ADT/StringRef.h"

namespace Commandline {

//===----------------------------------------------------------------------===//
// Aliased command line option (alias this name to a preexisting name)
//

class alias : public Option {
  Option* AliasFor;

  bool handleOccurrence(unsigned pos, llvm::StringRef /*ArgName*/,
                        llvm::StringRef Arg) override {
    return AliasFor->handleOccurrence(pos, AliasFor->ArgStr, Arg);
  }

  bool addOccurrence(unsigned pos, llvm::StringRef /*ArgName*/,
                     llvm::StringRef Value, bool MultiArg = false) override {
    return AliasFor->addOccurrence(pos, AliasFor->ArgStr, Value, MultiArg);
  }

  // Handle printing stuff...
  size_t getOptionWidth() const override;
  void printOptionInfo(size_t GlobalWidth) const override;

  // Aliases do not need to print their values.
  void printOptionValue(size_t /*GlobalWidth*/, bool /*Force*/) const override {
  }

  void setDefault() override { AliasFor->setDefault(); }

  ValueExpected getValueExpectedFlagDefault() const override {
    return AliasFor->getValueExpectedFlag();
  }

  void done() {
    if (!hasArgStr())
      error("cl::alias must have argument name specified!");
    if (!AliasFor)
      error("cl::alias must have an cl::aliasopt(option) specified!");
    if (!Subs.empty())
      error(
          "cl::alias must not have cl::sub(), aliased option's cl::sub() will "
          "be used!");
    Subs = AliasFor->Subs;
    Categories = AliasFor->Categories;
    addArgument();
  }

 public:
  // Command line options should not be copyable
  alias(const alias&) = delete;
  alias& operator=(const alias&) = delete;

  void setAliasFor(Option& O) {
    if (AliasFor)
      error("cl::alias must only have one cl::aliasopt(...) specified!");
    AliasFor = &O;
  }

  template <class... Mods>
  explicit alias(const Mods&... Ms)
      : Option(Optional, Hidden), AliasFor(nullptr) {
    apply(this, Ms...);
    done();
  }
};

// Modifier to set the option an alias aliases.
struct aliasopt {
  Option& Opt;

  explicit aliasopt(Option& O) : Opt(O) {}

  void apply(alias& A) const { A.setAliasFor(Opt); }
};

}  // namespace Commandline

#endif  // COMMANDLINE_ALIAS_H