#ifndef CONFIGURATION_HH_
#define CONFIGURATION_HH_
#include <any>
#include <cstdio>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
class Configurations {
protected:
  using parser_argument_iterator_t = std::vector<std::string>::const_iterator;
  using parser_t = std::function<std::any(parser_argument_iterator_t, parser_argument_iterator_t)>;
  struct Option {
    const parser_t parser;
    std::string    name;
    size_t         argument_count;

    Option(parser_t parser, std::string name, size_t argument_count)
      : parser(std::move(parser)), name(std::move(name)), argument_count(argument_count) {}
  };

  std::vector<Option> options;

  virtual void help() const = 0;

private:
  static auto
  parser_dispatcher(const Option &option, size_t &break_point, const std::vector<std::string> &args)
    -> std::any;
  static auto test_option(const Option &option, size_t &break_point, const std::vector<std::string> &args)
    -> std::any;

public:
  using parse_result_t = std::unordered_map<std::string, std::any>;

  Configurations()          = default;
  virtual ~Configurations() = default;

  // add a new option
  void add_option(const std::string &name, const parser_t &parser, size_t argument_count);

  // parse options from a structured vector of strings
  //  the termination point of argument parsing (index of the first non-argument entry) is returned in result
  //   named as _break_point and typed size_t, note the prefixing '_' which is intended to avoid conflicts
  [[nodiscard]] auto parse(const std::vector<std::string> &args) const -> parse_result_t;
  // parse options from raw command line, which is supplied to main as its argument
  //  see also parse(const std::vector<std::string> &args)const
  [[nodiscard]] auto parse(int argc, char **argv) const -> parse_result_t;

  // built-in parsers (and helpers) to simplify parsing of common types
  struct CommonParsers {
    CommonParsers() = delete;

    // convert string to unsigned integer, abort if the conversion failed or the string contains part that
    // cannot be converted
    template <typename T> static auto full_convert_unsigned(const std::string &value) -> T {
      size_t             pos = 0;
      unsigned long long result;
      try {
        result = std::stoi(value, std::addressof(pos));
      } catch (const std::exception &error) {
        pos = 0;
      }
      if (value.size() == 0 || pos == 0) {
        fprintf(stderr, "cannot convert %s.\n", value.c_str());
        exit(EXIT_FAILURE);
      }
      return static_cast<T>(result);
    }
    // produce true without taking any argument
    //  return value made by std::make_any<bool>(...)
    static auto true_parser(parser_argument_iterator_t begin, parser_argument_iterator_t end) -> std::any;
    // produce duration formatted as number of seconds with a single duration specification
    //  return value made by std::make_any<unsigned long long>(...)
    static auto duration_parser(parser_argument_iterator_t begin, parser_argument_iterator_t end) -> std::any;
    // produce size formatted as number of bytes with a single size specification
    //  return value made by std::make_any<unsigned long long>(...)
    static auto size_parser(parser_argument_iterator_t begin, parser_argument_iterator_t end) -> std::any;
    // produce result as is, that is store the input argument directly
    //  if there is only one argument supplied, return value made by std::make_any<std::string>
    //  otherwise, return value made by std::make_any<std::vector<std::string>>
    static auto identity_parser(parser_argument_iterator_t begin, parser_argument_iterator_t end) -> std::any;
  };
};
#endif