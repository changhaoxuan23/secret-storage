#ifndef UTILITY_HH_
#define UTILITY_HH_
#include <asm/termbits.h>
#include <iostream>
#include <print>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>
template <typename Allocator>
auto ask_secret(const char *prompt = "Enter secret",
                const char *retry_prompt = "Empty secret not allowed, enter again")
    -> std::basic_string<char, std::char_traits<char>, Allocator> {
  termios old_termios, new_termios;
  ioctl(STDIN_FILENO, TCGETS, &old_termios);
  new_termios = old_termios;
  new_termios.c_lflag &= ~(ECHO);
  ioctl(STDIN_FILENO, TCSETS, &new_termios);
  std::basic_string<char, std::char_traits<char>, Allocator> result;
  std::print("{}: ", prompt);
  while (true) {
    std::getline(std::cin, result);
    std::println("");
    if (result.size() == 0) {
      if (std::cin.eof()) {
        break;
      }
      std::print("{}: ", retry_prompt);
    } else {
      break;
    }
  }
  ioctl(STDIN_FILENO, TCSETS, &old_termios);
  return result;
}
#endif