#include "secret_storage_accessor.hh"
#include "hardened_memory_allocator.hh"
#include "message.hh"
#include "utility.hh"
#include <cstdlib>
#include <cstring>
#include <list>
#include <print>
#include <stdexcept>
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

static auto view_wrapper(secured_string &&string) -> std::string_view {
  secrets.emplace_front(string);
  secrets_map.emplace(secrets.front().data(), secrets.cbegin());
  return {secrets.front().data(), secrets.front().size()};
}

static auto send_message(void *data, size_t length) -> int {
  if (!initialized) {
    if (!SecretStorageAccessor::set_socket_path()) {
      return -1;
    }
  }
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

void SecretStorageAccessor::release_secured_string(std::string_view string) {
  secrets.erase(secrets_map.at(string.data()));
  secrets_map.erase(string.data());
}

auto SecretStorageAccessor::make_secured_key(size_t length) -> std::string_view {
  secured_string buffer(length, '\0');
  getrandom(buffer.data(), length, 0);
  return view_wrapper(std::move(buffer));
}

static inline auto to_hex(char c) -> char { return static_cast<char>(c > 9 ? c - 10 + 'A' : c + '0'); }
static inline auto from_hex(char c) -> uint8_t { return c >= '0' && c <= '9' ? c - '0' : c - 'A' + 10; }

auto SecretStorageAccessor::encode_string(std::string_view string) -> std::string_view {
  secured_string result(string.size() * 2, '\0');
  for (size_t i = 0; i < string.size(); i++) {
    result[i << 1]       = to_hex(static_cast<char>(string[i] >> 4));
    result[(i << 1) | 1] = to_hex(static_cast<char>(string[i] & 0xf));
  }
  return view_wrapper(std::move(result));
}

auto SecretStorageAccessor::decode_string(std::string_view string) -> std::string {
  if (string.size() & 1) {
    return {};
  }
  std::string result(string.size() >> 1, '\0');
  for (size_t i = 0; i < result.size(); i++) {
    result[i] = static_cast<char>((from_hex(string[i << 1]) << 4) | from_hex(string[(i << 1) | 1]));
  }
  return result;
}

auto SecretStorageAccessor::ask_secret(const char *prompt, const char *retry_prompt) -> std::string_view {
  auto result = ::ask_secret<secured_string::allocator_type>(prompt, retry_prompt);
  return view_wrapper(std::move(result));
}

auto SecretStorageAccessor::set_socket_path(const char *socket_path) -> bool {
  auto result = make_address(socket_path);
  if (result.has_value()) {
    address     = result.value();
    initialized = true;
    return true;
  }
  return false;
}

auto SecretStorageAccessor::ping() -> bool {
  output_message->type  = Message::Type::Ping;
  output_message->flags = 0;
  auto *const body      = reinterpret_cast<SingleEntryBody *>(output_message->data);
  body->length          = 128;
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

auto SecretStorageAccessor::exists(std::string_view key) -> bool {
  output_message->type  = Message::Type::Query;
  output_message->flags = Message::Flags::Query_ExistenceOnly;
  auto *const body      = reinterpret_cast<SingleEntryBody *>(output_message->data);
  body->length          = key.size();
  memcpy(body->data, key.data(), key.size());
  int socket_fd = send_message(output_message, sizeof(Message) + sizeof(SingleEntryBody) + body->length);
  if (socket_fd == -1) {
    return false;
  }
  recv(socket_fd, input_message, sizeof(Message), MSG_WAITALL);
  close(socket_fd);
  if (input_message->type == Message::Type::Ok) {
    return true;
  } else if (input_message->type == Message::Type::Failed) {
    return false;
  }
  throw std::logic_error("shall not reach here");
}

auto SecretStorageAccessor::submit_secret(std::string_view key, std::string_view value, bool replace)
  -> bool {
  output_message->type  = Message::Type::Add;
  output_message->flags = replace ? Message::Flags::Add_ReplaceExisting : 0;
  auto *const body      = reinterpret_cast<DoubleEntryBody *>(output_message->data);
  body->length[0]       = key.size();
  body->length[1]       = value.size();
  memcpy(body->data, key.data(), key.size());
  memcpy(body->data + key.size(), value.data(), value.size());
  int socket_fd = send_message(
    output_message, sizeof(Message) + sizeof(DoubleEntryBody) + body->length[0] + body->length[1]
  );
  if (socket_fd == -1) {
    return false;
  }
  recv(socket_fd, input_message, sizeof(Message), MSG_WAITALL);
  close(socket_fd);
  if (input_message->type == Message::Type::Ok) {
    return true;
  } else if (input_message->type == Message::Type::Failed) {
    return false;
  }
  throw std::logic_error("shall not reach here");
}

auto SecretStorageAccessor::remove_secret(std::string_view key, bool allow_missing) -> bool {
  output_message->type  = Message::Type::Delete;
  output_message->flags = allow_missing ? Message::Flags::Delete_AllowMissing : 0;
  auto *const body      = reinterpret_cast<SingleEntryBody *>(output_message->data);
  body->length          = key.size();
  memcpy(body->data, key.data(), key.size());
  int socket_fd = send_message(output_message, sizeof(Message) + sizeof(SingleEntryBody) + body->length);
  if (socket_fd == -1) {
    return false;
  }
  recv(socket_fd, input_message, sizeof(Message), MSG_WAITALL);
  close(socket_fd);
  if (input_message->type == Message::Type::Ok) {
    return true;
  } else if (input_message->type == Message::Type::Failed) {
    return false;
  }
  throw std::logic_error("shall not reach here");
}

void SecretStorageAccessor::terminate_server() {
  output_message->type  = Message::Type::Terminate;
  output_message->flags = 0;
  close(send_message(output_message, sizeof(Message)));
}

auto SecretStorageAccessor::get_secret(std::string_view key, SecretStorageAccessor::GetOption option)
  -> std::string_view {
  output_message->type  = Message::Type::Query;
  output_message->flags = option.remove_ ? Message::Flags::Query_DeleteSecret : 0;
  auto *const body      = reinterpret_cast<SingleEntryBody *>(output_message->data);
  body->length          = key.size();
  memcpy(body->data, key.data(), key.size());
  int socket_fd = send_message(output_message, sizeof(Message) + sizeof(SingleEntryBody) + body->length);
  if (socket_fd != -1) {
    recv(socket_fd, input_message, sizeof(Message), MSG_WAITALL);
    if (input_message->type == Message::Type::Result) {
      auto *const input_body = reinterpret_cast<SingleEntryBody *>(input_message->data);
      input_body->receive(socket_fd);
      close(socket_fd);
      return view_wrapper(secured_string(reinterpret_cast<char *>(input_body->data), input_body->length));
    }
    close(socket_fd);
  }
  if (!option.ask()) {
    return {};
  }
  auto result = SecretStorageAccessor::ask_secret(option.prompt_);
  if (result.size() == 0) {
    return result;
  }
  if (option.update_) {
    SecretStorageAccessor::submit_secret(key, result);
  }
  return result;
}

auto SecretStorageAccessor::ensure_secret(std::string_view key, const char *prompt) -> bool {
  if (!SecretStorageAccessor::ping()) {
    return false;
  }
  auto result = SecretStorageAccessor::get_secret(key, SecretStorageAccessor::GetOption().prompt(prompt));
  SecretStorageAccessor::release_secured_string(result.data());
  return result.size() != 0;
}