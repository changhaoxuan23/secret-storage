#include "secret_storage_accessor.hh"
#include "hardened_memory_allocator.hh"
#include "message.hh"
#include "utility.hh"
#include <cstdlib>
#include <cstring>
#include <list>
#include <print>
#include <sys/random.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <vector>

static bool                                                   initialized = false;
static std::vector<uint8_t, HardenedMemoryAllocator<uint8_t>> input_buffer(2000);
static std::vector<uint8_t, HardenedMemoryAllocator<uint8_t>> output_buffer(2000);
static Message    *input_message  = reinterpret_cast<Message *>(input_buffer.data());
static Message    *output_message = reinterpret_cast<Message *>(output_buffer.data());
static sockaddr_un address;

static std::list<secured_string>                                           secrets;
static std::unordered_map<const char *, decltype(secrets)::const_iterator> secrets_map;

static auto send_message(void *data, size_t length) -> int {
  int socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (socket_fd == -1) {
    return -1;
  }
  ssize_t return_value = connect(socket_fd, reinterpret_cast<const sockaddr *>(&address), sizeof(address));
  if (return_value == -1) {
    close(socket_fd);
    return -1;
  }
  return_value = send(socket_fd, data, length, MSG_NOSIGNAL);
  if (static_cast<size_t>(return_value) != length) {
    close(socket_fd);
    return -1;
  }
  return socket_fd;
}

static void update(const char *key, size_t key_length, const secured_string &value) {
  auto *const output_body = reinterpret_cast<DoubleEntryBody *>(output_message->data);
  output_message->type    = Message::Type::Update;
  output_body->length[0]  = key_length;
  memcpy(output_body->data, key, key_length);
  output_body->length[1] = value.size();
  memcpy(output_body->data + key_length, value.c_str(), value.size());
  int socket_fd = send_message(
    output_message,
    sizeof(Message) + sizeof(DoubleEntryBody) + output_body->length[0] + output_body->length[1]
  );
  if (socket_fd == -1) {
    return;
  }
  recv(socket_fd, input_message, sizeof(Message), MSG_WAITALL);
  // error check omitted
  close(socket_fd);
}

static auto query(const char *key, size_t key_length) -> secured_string {
  auto *const output_body = reinterpret_cast<SingleEntryBody *>(output_message->data);
  output_message->type    = Message::Type::Query;
  output_body->length     = key_length;
  memcpy(output_body->data, key, key_length);
  int socket_fd =
    send_message(output_message, sizeof(Message) + sizeof(SingleEntryBody) + output_body->length);
  if (socket_fd == -1) {
    return {};
  }
  recv(socket_fd, input_message, sizeof(Message), MSG_WAITALL);
  if (input_message->type != Message::Type::Result) {
    close(socket_fd);
    return {};
  }
  auto *const input_body = reinterpret_cast<SingleEntryBody *>(input_message->data);
  input_body->receive(socket_fd);
  close(socket_fd);
  return {reinterpret_cast<const char *>(input_body->data), input_body->length};
}

void SecretStorageAccessor::initialize(const char *socket_path) {
  address     = make_address(socket_path);
  initialized = true;
}

auto SecretStorageAccessor::ping() -> bool {
  if (!initialized) {
    SecretStorageAccessor::initialize();
  }
  output_message->type = Message::Type::Ping;
  auto *const body     = reinterpret_cast<SingleEntryBody *>(output_message->data);
  body->length         = 128;
  getrandom(body->data, body->length, 0);
  int socket_fd = send_message(output_message, sizeof(Message) + sizeof(SingleEntryBody) + body->length);
  if (socket_fd == -1) {
    return false;
  }
  recv(socket_fd, input_message, sizeof(Message), MSG_WAITALL);
  if (input_message->type != Message::Type::Pong) {
    close(socket_fd);
    return false;
  }
  auto *const input_body = reinterpret_cast<SingleEntryBody *>(input_message->data);
  input_body->receive(socket_fd);
  close(socket_fd);
  if (input_body->length != body->length) {
    return false;
  }
  return memcmp(input_body->data, body->data, body->length) == 0;
}

auto SecretStorageAccessor::make_secured_key(size_t length) -> std::string_view {
  secured_string buffer(length, '\0');
  getrandom(buffer.data(), length, 0);
  secrets.emplace_front(std::move(buffer));
  secrets_map.emplace(secrets.front().data(), secrets.cbegin());
  return {secrets.front().data(), secrets.front().size()};
}
auto SecretStorageAccessor::get_secret(const char *key, size_t length, const char *prompt)
  -> std::string_view {
  if (!initialized) {
    SecretStorageAccessor::initialize();
  }
  auto result = query(key, length);
  if (result.size() == 0) {
    if (prompt == nullptr) {
      result = ask_secret<secured_string::allocator_type>();
    } else {
      result = ask_secret<secured_string::allocator_type>(prompt);
    }
    if (result.size() != 0) {
      update(key, length, result);
    } else {
      return {};
    }
  }
  secrets.emplace_front(std::move(result));
  secrets_map.emplace(secrets.front().data(), secrets.cbegin());
  return {secrets.front().data(), secrets.front().size()};
}

void SecretStorageAccessor::release_secured_string(const char *string) {
  secrets.erase(secrets_map.at(string));
  secrets_map.erase(string);
}