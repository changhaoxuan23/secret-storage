#include "secret_storage_accessor.hh"
#include <configuration.hh>
#include <print>

class CommandLineParser : public Configurations {
public:
  CommandLineParser() {
    this->add_option("--socket", CommandLineParser::CommonParsers::identity_parser, 1);
    this->add_option("--get", CommandLineParser::CommonParsers::identity_parser, 1);
    this->add_option(
      "--key",
      [](Configurations::parser_argument_iterator_t begin, Configurations::parser_argument_iterator_t)
        -> std::any { return Configurations::CommonParsers::full_convert_unsigned<uint16_t>(*begin); },
      1
    );
  }
  void help() const override {
    std::println("Usage: test-accessor [OPTIONS...]                                                 ");
    std::println("OPTIONS:                                                                          ");
    std::println("                                                                                  ");
    std::println("  --socket     Specify the path of socket to communicate with server.             ");
    std::println("                                                                                  ");
    std::println("  --get        Get secret tagged with some key, ask if needed. Updates the server.");
    std::println("                                                                                  ");
    std::println("  --key        Make a secret key.                                                 ");
    std::println("                                                                                  ");
    std::println("  --help       Show this message again                                            ");
  }
};

auto main(int argc, char **argv) -> int {
  auto options = CommandLineParser().parse(argc, argv);
  if (options.contains("socket")) {
    SecretStorageAccessor::initialize(std::any_cast<std::string>(options.at("socket")).c_str());
  }
  if (options.contains("get") && options.contains("key")) {
    std::println(stderr, "specify no more than one of get/key");
    return 1;
  }
  if (options.contains("get")) {
    const auto &key    = std::any_cast<std::string>(options.at("get"));
    const auto  secret = SecretStorageAccessor::get_secret(key.c_str(), key.size());
    if (secret.size() == 0) {
      std::println("failed to get secret");
      return 0;
    }
    std::println("secret: {}", secret);
    SecretStorageAccessor::release_secured_string(secret.data());
  } else if (options.contains("key")) {
    const auto &length = std::any_cast<uint16_t>(options.at("key"));
    const auto  key    = SecretStorageAccessor::make_secured_key(length);
    std::println("key: {}", key);
    SecretStorageAccessor::release_secured_string(key.data());
  }
  return 0;
}