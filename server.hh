#ifndef SERVER_HH_
#define SERVER_HH_
#include "storage.hh"
#include <filesystem>
class Server {
public:
  static auto build(Storage &storage) -> Server &;

  ~Server();
  auto start(const char *address = nullptr) -> bool;
  void serve();

private:
  Storage &storage;
  int socket_fd{-1};
  std::filesystem::path address;
  bool running{true};

  Server(Storage &storage);
};
#endif