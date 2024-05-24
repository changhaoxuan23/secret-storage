#include "command_line.hh"
#include <print>
CommandLineParser::CommandLineParser() {
  this->add_option("--daemon", CommandLineParser::CommonParsers::true_parser, 0);
  this->add_option("--socket", CommandLineParser::CommonParsers::identity_parser, 1);
  this->add_option("--manual-initialize", CommandLineParser::CommonParsers::true_parser, 0);
}
void CommandLineParser::help() const {
  std::println("In memory storage to hold secrets                                                          ");
  std::println("Usage: secret-storage [OPTIONS...]                                                         ");
  std::println("OPTIONS:                                                                                   ");
  std::println("  --daemon              Run as a daemon, which will make this program to fork to background");
  std::println("                          all console output are disabled                                  ");
  std::println("                                                                                           ");
  std::println("  --socket              Specify the path to create socket for communication with clients.  ");
  std::println("                         This path must not refer to a target that exists, or this program ");
  std::println("                          will refuse to start up.                                         ");
  std::println("                                                                                           ");
  std::println("  --manual-initialize   Request to initialize some entries manually into the storage.      ");
  std::println("                                                                                           ");
  std::println("  --help                Show this message again                                            ");
}