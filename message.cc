#include "message.hh"
#include <cstdlib>
#include <filesystem>
#include <print>
#include <stack>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

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

static auto make_directories(const std::filesystem::path &path) -> bool {
  if (std::filesystem::exists(path)) {
    std::println(stderr, "{} exists!", path.c_str());
    return false;
  }
  std::stack<std::filesystem::path> paths;

  auto helper = path;
  while (helper.has_parent_path()) {
    helper = helper.parent_path();

    auto status = std::filesystem::status(helper);
    if (std::filesystem::exists(status)) {
      if (!std::filesystem::is_directory(status)) {
        std::println(stderr, "parent element {} is not a directory!", path.c_str());
        return false;
      }
      break;
    }
    paths.push(helper);
  }
  while (!paths.empty()) {
    const auto &directory = paths.top();
    if (::mkdir(directory.c_str(), 0700) == -1) {
      std::println(stderr, "failed to create {}: {}", directory.c_str(), strerror(errno));
      return false;
    }
    paths.pop();
  }
  return true;
}

static auto check_socket_security(const std::filesystem::path &path) -> bool {
  auto        helper = path.parent_path();
  struct stat status;
  if (::stat(helper.c_str(), &status) == -1) {
    std::println(stderr, "failed to stat {}: {}", helper.c_str(), strerror(errno));
    return false;
  }
  auto uid = ::getuid();
  auto gid = ::getgid();
  bool fix = false;
  if (status.st_uid != uid) {
    std::println(stderr, "incorrect owner uid: expected {}, got {}, will try to fix", uid, status.st_uid);
    fix = true;
  }
  if (status.st_gid != gid) {
    std::println(stderr, "incorrect owner gid: expected {}, got {}, will try to fix", gid, status.st_gid);
    fix = true;
  }
  if (fix && ::chown(helper.c_str(), uid, gid) == -1) {
    std::println(stderr, "failed to fix {}: {}", helper.c_str(), strerror(errno));
    return false;
  }
  if ((status.st_mode & 0777) != 0700) {
    std::println(stderr, "insecure mode: expected {:o}, got {:o}, will try to fix", 0777, status.st_mode);
    if (::chmod(helper.c_str(), 0777) == -1) {
      std::println(stderr, "failed to fix {}: {}", helper.c_str(), strerror(errno));
      return false;
    }
  }
  return true;
}

auto make_address(const char *path, bool create) -> std::optional<sockaddr_un> {
  sockaddr_un address;
  address.sun_family = AF_UNIX;
  if (path != nullptr) {
    if (create && !make_directories(path)) {
      return {};
    }
    if (!check_socket_security(path)) {
      return {};
    }
    strcpy(address.sun_path, path);
    return address;
  }
  const auto *directory = getenv("XDG_RUNTIME_DIR");
  if (directory != nullptr) {
    std::filesystem::path helper(directory);
    helper /= DefaultSocketName;
    if (!check_socket_security(helper)) {
      return {};
    }
    strcpy(address.sun_path, helper.c_str());
    return address;
  }
  directory = getenv("HOME");
  if (directory != nullptr) {
    std::filesystem::path helper(directory);
    helper /= ".local/run/" DefaultSocketName;
    if (create && !make_directories(helper)) {
      return {};
    }
    if (!check_socket_security(helper)) {
      return {};
    }
    strcpy(address.sun_path, helper.c_str());
    return address;
  }
  auto helper = std::filesystem::absolute(DefaultSocketName);
  if (!check_socket_security(helper)) {
    return {};
  }
  strcpy(address.sun_path, DefaultSocketName);
  return address;
}