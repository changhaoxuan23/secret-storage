#ifndef SECRET_STORAGE_ACCESSOR_HH_
#define SECRET_STORAGE_ACCESSOR_HH_
#include <cstddef>
#include <string_view>
namespace SecretStorageAccessor {
void initialize(const char *socket_path = nullptr);
auto ping() -> bool;
auto make_secured_key(size_t length) -> std::string_view;
auto get_secret(const char *key, size_t length, const char *prompt = nullptr) -> std::string_view;
void release_secured_string(const char *string);
} // namespace SecretStorageAccessor
#endif