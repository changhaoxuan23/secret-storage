#ifndef SECRET_STORAGE_ACCESSOR_HH_
#define SECRET_STORAGE_ACCESSOR_HH_
#include <cstddef>
#include <string_view>
namespace SecretStorageAccessor {
// ---- begin of local utilities that does not depends on a running server ----

// release a secured string
// any function that returns a std::string_view is returning a view to the secured string stored in a static
//  local storage that must be freed by passing it back to SecretStorageAccessor::release_secured_string,
//  unless an empty view is returned in which case it should be treated as returning a nullptr
void release_secured_string(std::string_view string);

// make a randomly generated key that is stored in locked memory page
//  note that the key is generated in binary from that will not be a valid string under any encoding
//  it should be fine to use keys not stored in locked memory areas if they are generated randomly
auto make_secured_key(size_t length) -> std::string_view;

// hex (base16) encode a string
auto encode_string(std::string_view string) -> std::string_view;

// hex (base16) decode a string
//  note that in no case shall a string in locked memory being decoded, therefore this function returns
//   only a normal string that is not stored in locked memory area and shall not be freed
//   that is, you have the ownership of it
auto decode_string(std::string_view string) -> std::string;

// ask user to enter a secret via stdin/stdout
//  the result is not attached with a key and is not stored to be returned for further ask_secret calls
//  calling to this function is always interactive that requires the user to input something
auto ask_secret(const char *prompt, const char *retry_prompt = nullptr) -> std::string_view;

// ----  end of local utilities  ----

// set the path of socket used to communicate with secret storage server
//  calling this function with nullptr will cause the default socket path being selected for use
//  note that this function will be called automatically with nullptr if before connecting to the server if an
//   address is not previously specified, so you can simply do nothing to use the default path
//  this function can be called at any point to change the socket path
//  return true if the socket path is valid, false otherwise
auto set_socket_path(const char *socket_path = nullptr) -> bool;

// low-level server accessors

// check if a server is up and running
auto ping() -> bool;
// check if a secret is registered on the server
auto exists(std::string_view key) -> bool;
// set a secret directly to server
auto submit_secret(std::string_view key, std::string_view value, bool replace = false) -> bool;
// delete a secret on server
auto remove_secret(std::string_view key, bool allow_missing = false) -> bool;
// terminate the server
void terminate_server();

// high-level APIs
struct GetOption {
  const char *prompt_{nullptr};
  bool        update_{true};
  bool        remove_{false};

  [[nodiscard]] inline auto ask() const -> bool { return this->prompt_ != nullptr; }

  inline auto prompt(const char *prompt) -> GetOption & {
    this->prompt_ = prompt;
    return *this;
  }
  inline auto update(bool update) -> GetOption & {
    this->update_ = update;
    return *this;
  }
  inline auto remove(bool remove) -> GetOption & {
    this->remove_ = remove;
    return *this;
  }
};
// get a secret
//  try to retrieve the secret from server with the key
//  if that key does not exist on the server or server is down, ask user about it if prompt is not nullptr
//  if the secret is acquired by asking the user and update is true, submit the secret to server
//  if the secret is acquired from server and remove is true, remove it from server
auto get_secret(std::string_view key, GetOption option) -> std::string_view;
// ensure that a secret exists on the server
//  if the secret does not exist on the server, ask user for it and upload it
//  return true if the server finally holds the secret, false otherwise
auto ensure_secret(std::string_view key, const char *prompt) -> bool;
} // namespace SecretStorageAccessor
#endif