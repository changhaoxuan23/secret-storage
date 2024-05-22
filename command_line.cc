#include "command_line.hh"
CommandLineParser::CommandLineParser() {
  this->add_option("--daemon", CommandLineParser::CommonParsers::true_parser, 0);
  this->add_option("--replace", CommandLineParser::CommonParsers::true_parser, 0);
  this->add_option("--socket", CommandLineParser::CommonParsers::identity_parser, 1);
  this->add_option("--manual-initialize", CommandLineParser::CommonParsers::true_parser, 0);
}
void CommandLineParser::help() const {
  printf("In memory storage to hold secrets                                                          \n");
  printf("Usage: secret-storage [OPTIONS...]                                                         \n");
  printf("OPTIONS:                                                                                   \n");
  printf("  --daemon              Run as a daemon, which will make this program to fork to background\n");
  printf("                          all console output are disabled                                  \n");
  printf("                                                                                           \n");
  printf("  --replace             Try to replace it if there is another running instance, taking over\n");
  printf("                          all secrets handled by that instance.                            \n");
  printf("                         By default this program shall terminate its startup procedure if  \n");
  printf("                          another running instance is detected.                            \n");
  printf("                         Racing conditions in which multiple instances are launched within \n");
  printf("                          a short duration can be avoided by following the default settings\n");
  printf("                          but not with this option enabled, so use it with caution.        \n");
  printf("                                                                                           \n");
  printf("  --socket              Specify the path to create socket for communication with clients.  \n");
  printf("                         Unless --replace is supplied, this path must not refer to a target\n");
  printf("                          that exists, or this program will refuse to start up.            \n");
  printf("                         If --replace is supplied, this path may either refer to nothing or\n");
  printf("                          an existing socket, otherwise it will not start anyway.          \n");
  printf("                         Defaults to $XDG_RUNTIME_DIR/secret-storage.socket                \n");
  printf("                                                                                           \n");
  printf("  --manual-initialize   Request to initialize some entries manually into the storage.      \n");
  printf("                                                                                           \n");
  printf("  --help                Show this message again                                            \n");
}