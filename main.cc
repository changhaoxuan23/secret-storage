#include "command_line.hh"
#include "server.hh"
#include "storage.hh"
#include "utility.hh"
#include <any>
#include <iostream>
#include <print>
#include <unistd.h>

auto main(int argc, char **argv) -> int {
  const auto parser        = CommandLineParser();
  const auto configuration = parser.parse(argc, argv);

  Storage storage;

  auto server = Server::build(storage);

  if (configuration.contains("manual-initialize")) {
    secured_string key;
    secured_string value;
    std::println("Manual initializing secrets...");
    while (!std::cin.eof()) {
      std::print("  key (terminate by giving EOF): ");
      std::getline(std::cin, key);
      if (key.size() == 0) {
        if (std::cin.eof()) {
          std::println("");
          break;
        }
        std::println("  !! empty key is invalid !!");
        continue;
      }
      value = ask_secret<secured_string::allocator_type>();
      if (value.size() == 0) {
        break;
      }
      storage.add(std::move(key), std::move(value));
    }
  }
  fclose(stdin);
  bool start_status = true;
  if (configuration.contains("socket")) {
    start_status = server.start(std::any_cast<std::string>(configuration.at("socket")).c_str());
  } else {
    start_status = server.start();
  }
  if (start_status == false) {
    std::println(stderr, "failed to start server!");
    return 0;
  }
  if (configuration.contains("daemon")) {
    pid_t pid = fork();
    if (pid == -1) {
      std::println(stderr, "failed to daemonize, {}", strerror(errno));
      return -1;
    }
    if (pid == 0) {
      setsid();
      pid_t pid = fork();
      if (pid == -1) {
        std::println(stderr, "failed to daemonize, {}", strerror(errno));
        return -1;
      }
      if (pid != 0) {
        exit(EXIT_SUCCESS);
      }
      fclose(stdout);
      fclose(stderr);
    } else {
      exit(EXIT_SUCCESS);
    }
  }
  server.serve();
  return 0;
}