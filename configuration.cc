#include "configuration.hh"
#include <algorithm>
#include <cassert>
static auto get_size_suffix_map() -> const std::unordered_map<std::string, unsigned long long> & {
  static bool                                                initialize = true;
  static std::unordered_map<std::string, unsigned long long> map;
  if (initialize) {
    map.insert(std::make_pair("kib", 1024ull));
    map.insert(std::make_pair("kb", 1024ull));
    map.insert(std::make_pair("k", 1024ull));

    map.insert(std::make_pair("mib", 1024ull * 1024));
    map.insert(std::make_pair("mb", 1024ull * 1024));
    map.insert(std::make_pair("m", 1024ull * 1024));

    map.insert(std::make_pair("gib", 1024ull * 1024 * 1024));
    map.insert(std::make_pair("gb", 1024ull * 1024 * 1024));
    map.insert(std::make_pair("g", 1024ull * 1024 * 1024));

    map.insert(std::make_pair("tib", 1024ull * 1024 * 1024 * 1024));
    map.insert(std::make_pair("tb", 1024ull * 1024 * 1024 * 1024));
    map.insert(std::make_pair("t", 1024ull * 1024 * 1024 * 1024));

    map.insert(std::make_pair("pib", 1024ull * 1024 * 1024 * 1024 * 1024));
    map.insert(std::make_pair("pb", 1024ull * 1024 * 1024 * 1024 * 1024));
    map.insert(std::make_pair("p", 1024ull * 1024 * 1024 * 1024 * 1024));
  }
  return map;
}
static auto get_duration_suffix_map() -> const std::unordered_map<std::string, unsigned long long> & {
  static bool                                                initialize = true;
  static std::unordered_map<std::string, unsigned long long> map;
  if (initialize) {
    map.insert(std::make_pair("m", 60ull));
    map.insert(std::make_pair("minute", 60ull));
    map.insert(std::make_pair("minutes", 60ull));

    map.insert(std::make_pair("h", 60ull * 60));
    map.insert(std::make_pair("hour", 60ull * 60));
    map.insert(std::make_pair("hours", 60ull * 60));

    map.insert(std::make_pair("d", 60ull * 60 * 24));
    map.insert(std::make_pair("day", 60ull * 60 * 24));
    map.insert(std::make_pair("days", 60ull * 60 * 24));
  }
  return map;
}
auto Configurations::CommonParsers::true_parser(
  parser_argument_iterator_t begin, parser_argument_iterator_t end
) -> std::any {
  assert(begin == end);
  return std::make_any<bool>(true);
}
auto Configurations::CommonParsers::duration_parser(
  parser_argument_iterator_t begin, parser_argument_iterator_t end
) -> std::any {
  assert(std::next(begin) == end);
  unsigned long long target;
  size_t             pos = 0;
  try {
    target = std::stoull(*begin, std::addressof(pos));
  } catch (const std::exception &e) {
    fprintf(stderr, "invalid value %s\n", begin->c_str());
    exit(EXIT_FAILURE);
  }
  if (pos != begin->size()) {
    auto suffix = begin->substr(pos);
    std::for_each(suffix.begin(), suffix.end(), [](auto &c) { c = tolower(c); });
    const auto &map  = get_duration_suffix_map();
    auto        iter = map.find(suffix);
    if (iter == map.cend()) {
      fprintf(stderr, "invalid suffix %s\n", begin->substr(pos).c_str());
      exit(EXIT_FAILURE);
    }
    target *= iter->second;
  }
  return std::make_any<unsigned long long>(target);
}
auto Configurations::CommonParsers::size_parser(
  parser_argument_iterator_t begin, parser_argument_iterator_t end
) -> std::any {
  assert(std::next(begin) == end);
  unsigned long long target;
  size_t             pos = 0;
  try {
    target = std::stoull(*begin, std::addressof(pos));
  } catch (const std::exception &e) {
    fprintf(stderr, "invalid value %s\n", begin->c_str());
    exit(EXIT_FAILURE);
  }
  if (pos != begin->size()) {
    auto suffix = begin->substr(pos);
    std::for_each(suffix.begin(), suffix.end(), [](auto &c) { c = tolower(c); });
    const auto &map  = get_size_suffix_map();
    auto        iter = map.find(suffix);
    if (iter == map.cend()) {
      fprintf(stderr, "invalid suffix %s\n", begin->substr(pos).c_str());
      exit(EXIT_FAILURE);
    }
    target *= iter->second;
  }
  return std::make_any<unsigned long long>(target);
}
auto Configurations::CommonParsers::identity_parser(
  parser_argument_iterator_t begin, parser_argument_iterator_t end
) -> std::any {
  assert(begin != end);
  if (std::distance(begin, end) == 1) {
    return std::make_any<std::string>(*begin);
  } else {
    return std::make_any<std::vector<std::string>>(std::vector<std::string>{begin, end});
  }
}

auto Configurations::parser_dispatcher(
  const Option &option, size_t &break_point, const std::vector<std::string> &args
) -> std::any {
  if (args[break_point] == option.name) {
    if (args.size() <= break_point + option.argument_count) {
      fprintf(stderr, "%s expects arguments but not provided\n", option.name.c_str());
      exit(EXIT_FAILURE);
    }
    auto step = static_cast<parser_argument_iterator_t::difference_type>(++break_point);
    break_point += option.argument_count;
    return option.parser(
      std::next(args.cbegin(), step),
      std::next(args.cbegin(), step + static_cast<decltype(step)>(option.argument_count))
    );
  } else if (option.argument_count == 1 && args[break_point].substr(0, option.name.size() + 1) == (option.name + '=')) {
    std::vector<std::string> temporary;
    temporary.emplace_back(args[break_point++].substr(option.name.size() + 1));
    return option.parser(temporary.cbegin(), temporary.cend());
  } else {
    fprintf(
      stderr, "invalid option string %s with option name %s\n", args[break_point].c_str(), option.name.c_str()
    );
    exit(EXIT_FAILURE);
  }
}
auto Configurations::test_option(
  const Option &option, size_t &break_point, const std::vector<std::string> &args
) -> std::any {
  if (args[break_point].substr(0, option.name.size()) == option.name) {
    return Configurations::parser_dispatcher(option, break_point, args);
  }
  // return a empty std::any instance
  return {};
}

void Configurations::add_option(const std::string &name, const parser_t &parser, size_t argument_count) {
  this->options.emplace_back(parser, name, argument_count);
}

auto Configurations::parse(const std::vector<std::string> &args) const -> Configurations::parse_result_t {
  Configurations::parse_result_t result;
  size_t                         break_point = 1;
  while (break_point < args.size() && args[break_point].substr(0, 2).compare("--") == 0) {
    if (args[break_point].size() == 2) {
      // it is just '--'
      ++break_point;
      break;
    }
    bool matched = false;
    for (const auto &option : this->options) {
      auto &&parse_result = Configurations::test_option(option, break_point, args);
      if (parse_result.has_value()) {
        matched = true;
        result.insert_or_assign(option.name.substr(2), parse_result);
        break;
      }
    }
    if (!matched) {
      // automatically add a implicit option of --help
      if (args[break_point] == "--help") {
        this->help();
        exit(EXIT_SUCCESS);
      }
      fprintf(stderr, "unrecognized option %s\n", args[break_point].c_str());
      this->help();
      exit(EXIT_FAILURE);
    }
  }
  // add break point to result
  result.insert(std::make_pair("_break_point", std::make_any<size_t>(break_point)));

  return result;
}
auto Configurations::parse(int argc, char **argv) const -> Configurations::parse_result_t {
  return this->parse({argv, argv + argc});
}