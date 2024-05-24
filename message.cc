#include "message.hh"
#include <cstdlib>
#include <filesystem>
#include <sys/socket.h>

#ifndef DefaultSocketName
#define DefaultSocketName "secret-storage.sock"
#endif

void SingleEntryBody::receive(int socket_fd) {
  recv(socket_fd, &this->length, sizeof(this->length), MSG_WAITALL);
  recv(socket_fd, this->data, this->length, MSG_WAITALL);
}
void DoubleEntryBody::receive(int socket_fd) {
  recv(socket_fd, this->length, sizeof(this->length), MSG_WAITALL);
  recv(socket_fd, this->data, this->length[0] + this->length[1], MSG_WAITALL);
}

auto make_address(const char *path) -> sockaddr_un {
  sockaddr_un address;
  address.sun_family = AF_UNIX;
  if (path != nullptr) {
    std::filesystem::path helper(path);
    std::filesystem::create_directories(helper.parent_path());
    strcpy(address.sun_path, path);
    return address;
  }
  const auto *directory = getenv("XDG_RUNTIME_DIR");
  if (directory != nullptr) {
    std::filesystem::path helper(directory);
    helper /= DefaultSocketName;
    strcpy(address.sun_path, helper.c_str());
    return address;
  }
  directory = getenv("HOME");
  if (directory != nullptr) {
    std::filesystem::path helper(directory);
    helper /= ".local/run/" DefaultSocketName;
    std::filesystem::create_directories(helper.parent_path());
    strcpy(address.sun_path, helper.c_str());
    return address;
  }
  strcpy(address.sun_path, DefaultSocketName);
  return address;
}