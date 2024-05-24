#ifndef MESSAGE_HH_
#define MESSAGE_HH_
#include <cstdint>
#include <sys/un.h>

struct Message {
  enum Type : uint8_t {
    Ping,
    Pong,
    Add,
    Update,
    Query,
    Delete,
    Ok,
    Failed,
    Result,
  } type;
  uint8_t data[];
};
struct SingleEntryBody {
  uint16_t length;
  uint8_t data[];
  void receive(int socket_fd);
};
struct DoubleEntryBody {
  uint16_t length[2];
  uint8_t data[];
  void receive(int socket_fd);
};

auto make_address(const char *path = nullptr) -> sockaddr_un;
#endif