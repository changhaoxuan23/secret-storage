#include "server.hh"
#include "message.hh"
#include <csignal>
#include <cstring>
#include <print>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

void dummy_handler(int) {}

auto Server::build(Storage &storage) -> Server & {
  static Server instance(storage);
  return instance;
}

Server::Server(Storage &storage) : storage(storage) {}
Server::~Server() {
  close(this->socket_fd);
  std::filesystem::remove(this->address);
}

auto Server::start(const char *address) -> bool {
  sockaddr_un unix_socket_address = make_address(address);
  this->socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (this->socket_fd == -1) {
    return false;
  }
  int return_value = bind(this->socket_fd, reinterpret_cast<const struct sockaddr *>(&unix_socket_address),
                          sizeof(unix_socket_address));
  if (return_value == -1) {
    return false;
  }
  this->address = unix_socket_address.sun_path;
  return_value = listen(this->socket_fd, 5);
  if (return_value == -1) {
    return false;
  }
  struct sigaction action;
  action.sa_handler = dummy_handler;
  action.sa_flags = 0;
  sigemptyset(&action.sa_mask);
  sigaction(SIGINT, &action, nullptr);
  return true;
}

void Server::serve() {
  constexpr size_t buffer_size = 2000;
  HardenedMemoryAllocator<uint8_t> allocator;
  auto *const input_message = reinterpret_cast<Message *>(allocator.allocate(buffer_size));
  auto *const output_message = reinterpret_cast<Message *>(allocator.allocate(buffer_size));
  while (this->running) {
    int pair_socket = accept(this->socket_fd, nullptr, nullptr);
    if (pair_socket == -1) {
      if (errno == EINTR) {
        break;
      }
      continue;
    }
    recv(pair_socket, input_message, sizeof(Message), MSG_WAITALL);
    size_t result_length = sizeof(Message);

    if (input_message->type == Message::Type::Ping) {
      output_message->type = Message::Type::Pong;
      auto *const input = reinterpret_cast<SingleEntryBody *>(input_message->data);
      auto *const output = reinterpret_cast<SingleEntryBody *>(output_message->data);
      input->receive(pair_socket);
      output->length = input->length;
      memcpy(output->data, input->data, input->length);
      result_length += sizeof(SingleEntryBody) + output->length;
    } else if (input_message->type == Message::Type::Add) {
      auto *const input = reinterpret_cast<DoubleEntryBody *>(input_message->data);
      input->receive(pair_socket);
      const auto result = this->storage.add(
          secured_string(reinterpret_cast<const char *>(input->data), input->length[0]),
          secured_string(reinterpret_cast<const char *>(input->data + input->length[0]), input->length[1]));
      if (result) {
        output_message->type = Message::Type::Ok;
      } else {
        output_message->type = Message::Type::Failed;
      }
    } else if (input_message->type == Message::Type::Update) {
      auto *const input = reinterpret_cast<DoubleEntryBody *>(input_message->data);
      input->receive(pair_socket);
#ifndef NDEBUG
      std::println(
          "UPDATE({}, {})", std::string(reinterpret_cast<const char *>(input->data), input->length[0]),
          std::string(reinterpret_cast<const char *>(input->data + input->length[0]), input->length[1]));
#endif
      this->storage.update(
          secured_string(reinterpret_cast<const char *>(input->data), input->length[0]),
          secured_string(reinterpret_cast<const char *>(input->data + input->length[0]), input->length[1]));
      output_message->type = Message::Type::Ok;
    } else if (input_message->type == Message::Type::Query) {
      auto *const input = reinterpret_cast<SingleEntryBody *>(input_message->data);
      input->receive(pair_socket);
#ifndef NDEBUG
      std::print("QUERY({})", std::string(reinterpret_cast<const char *>(input->data), input->length));
#endif
      const auto result =
          this->storage.query(secured_string(reinterpret_cast<const char *>(input->data), input->length));
      if (result == nullptr) {
#ifndef NDEBUG
        std::println(" -> No Such Entry");
#endif
        output_message->type = Message::Type::Failed;
      } else {
#ifndef NDEBUG
        std::println(" -> {}", std::string(result->c_str(), result->size()));
#endif
        output_message->type = Message::Type::Result;
        auto *const output = reinterpret_cast<SingleEntryBody *>(output_message->data);
        output->length = result->size();
        memcpy(output->data, result->c_str(), output->length);
        result_length += sizeof(SingleEntryBody) + output->length;
      }
    } else if (input_message->type == Message::Type::Delete) {
      output_message->type = Message::Type::Failed;
    }

    send(pair_socket, output_message, result_length, 0);
    close(pair_socket);
  }
  allocator.deallocate(reinterpret_cast<uint8_t *>(input_message), -1);
  allocator.deallocate(reinterpret_cast<uint8_t *>(output_message), -1);
}