#ifndef COMMAND_LINE_HH_
#define COMMAND_LINE_HH_
#include "configuration.hh"
class CommandLineParser : public Configurations {
protected:
  void help() const override;

public:
  CommandLineParser();
  ~CommandLineParser() = default;
};
#endif