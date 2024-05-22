#include "command_line.hh"
#include <any>
#include <iostream>
template <typename T>
void print_option(const CommandLineParser::parse_result_t &result, const std::string &name) {
  std::cout << name << ": ";
  const auto &target = result.find(name);
  if (target == result.cend()) {
    std::cout << "not exist\n";
  } else {
    std::cout << std::any_cast<T>(target->second) << '\n';
  }
}
auto main(int argc, char **argv) -> int {
  const auto parser        = CommandLineParser();
  const auto configuration = parser.parse(argc, argv);
  print_option<bool>(configuration, "daemon");
  print_option<bool>(configuration, "replace");
  print_option<std::string>(configuration, "socket");
  print_option<bool>(configuration, "manual-initialize");
  return 0;
}