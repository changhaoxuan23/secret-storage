#include "secret_storage_accessor.hh"
#include <configuration.hh>
#include <print>

class Parser : public Configurations {
public:
  Parser() {
    this->add_option("--socket", Configurations::CommonParsers::identity_parser, 1);
    this->add_option("--ping", Configurations::CommonParsers::true_parser, 0);
    this->add_option("--terminate", Configurations::CommonParsers::true_parser, 0);
    this->add_option("--hex", Configurations::CommonParsers::true_parser, 0);
    this->add_option("--get", Configurations::CommonParsers::identity_parser, 1);
    this->add_option("--check", Configurations::CommonParsers::identity_parser, 1);
    this->add_option("--set", Configurations::CommonParsers::identity_parser, 1);
    this->add_option("--delete", Configurations::CommonParsers::identity_parser, 1);
  }
  void help() const override {
    std::println("secret-control-ctl v{}, command line interface to secret-storage server", VERSION);
    std::println(" This program is part of secret-storage                                         ");
    std::println("                                                                                ");
    std::println("Usage: secret-control-ctl [OPTIONS...]                                          ");
    std::println("OPTIONS:                                                                        ");
    std::println("  --socket PATH  Specify the path of socket file to connect with server.        ");
    std::println("                                                                                ");
    std::println("  --ping         Check if a server is up and running.                           ");
    std::println("                                                                                ");
    std::println("  --terminate    Terminate the server.                                          ");
    std::println("                                                                                ");
    std::println("  --hex          Indicate the KEY specified is base16 encoded.                  ");
    std::println("                                                                                ");
    std::println("  --get    KEY   Get secret value associated with the KEY.                      ");
    std::println("                                                                                ");
    std::println("  --check  KEY   Check if a value associated with the KEY exists on server.     ");
    std::println("                                                                                ");
    std::println("  --set    KEY   Store a secret to the server. The value is specified via stdin.");
    std::println("                                                                                ");
    std::println("  --delete KEY   Delete secret value associated with the KEY.                   ");
  }
};

auto main(int argc, char **argv) -> int {
  auto options = Parser().parse(argc, argv);
  if (options.contains("socket")) {
    if (!SecretStorageAccessor::set_socket_path(std::any_cast<std::string>(options.at("socket")).c_str())) {
      std::println("cannot use this socket path due to security consideration");
      return 0;
    }
  }
  if (options.contains("ping")) {
    if (SecretStorageAccessor::ping()) {
      std::println("pong");
    } else {
      std::println("biu~");
    }
  } else if (options.contains("terminate")) {
    SecretStorageAccessor::terminate_server();
  } else {
    bool        hex = options.contains("hex");
    std::string key;
    if (options.contains("get")) {
      key = std::any_cast<std::string>(options.at("get"));
    } else if (options.contains("check")) {
      key = std::any_cast<std::string>(options.at("check"));
    } else if (options.contains("set")) {
      key = std::any_cast<std::string>(options.at("set"));
    } else if (options.contains("delete")) {
      key = std::any_cast<std::string>(options.at("delete"));
    }
    if (hex) {
      key = SecretStorageAccessor::decode_string(key);
    }
    if (options.contains("get")) {
      auto result =
        SecretStorageAccessor::get_secret(key, SecretStorageAccessor::GetOption().prompt(nullptr));
      if (result.empty()) {
        std::println("--x \x1b[1;31mNot Exist\x1b[0m");
      } else {
        std::println("--> {}", result);
        SecretStorageAccessor::release_secured_string(result);
      }
    } else if (options.contains("check")) {
      auto result = SecretStorageAccessor::exists(key);
      if (result) {
        std::println("--> exists");
      } else {
        std::println("--> nope");
      }
    } else if (options.contains("set")) {
      auto result  = SecretStorageAccessor::ask_secret("Enter secret value");
      auto succeed = SecretStorageAccessor::submit_secret(key, result);
      SecretStorageAccessor::release_secured_string(result);
      if (succeed) {
        std::println("--> ok");
      } else {
        std::println("--> failed");
      }
    } else if (options.contains("delete")) {
      auto result = SecretStorageAccessor::remove_secret(key);
      if (result) {
        std::println("--> ok");
      } else {
        std::println("--> failed");
      }
    }
  }
  return 0;
}