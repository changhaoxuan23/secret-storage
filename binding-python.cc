#include "secret_storage_accessor.hh"
#include <optional>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

static void set_address(const char *socket_path) { SecretStorageAccessor::initialize(socket_path); }

static auto get_secret_proxy(const char *key, size_t length, const char *prompt = nullptr)
  -> std::optional<pybind11::memoryview> {
  auto result = SecretStorageAccessor::get_secret(key, length, prompt);
  if (result.size() == 0) {
    return {};
  }
  return pybind11::memoryview::from_memory(
    const_cast<char *>(result.data()), static_cast<long>(result.size()), true
  );
}

static auto make_secured_key(size_t length) -> pybind11::memoryview {
  auto result = SecretStorageAccessor::make_secured_key(length);
  return pybind11::memoryview::from_memory(
    const_cast<char *>(result.data()), static_cast<long>(length), true
  );
}

static auto get_secret_plain(pybind11::bytes key, const char *prompt) -> std::optional<pybind11::memoryview> {
  std::string key_string(key);
  return get_secret_proxy(key_string.c_str(), key_string.size(), prompt);
}
static auto get_secret_hidden(pybind11::memoryview key, const char *prompt)
  -> std::optional<pybind11::memoryview> {
  auto key_instance = PyMemoryView_GET_BUFFER(key.ptr());
  return get_secret_proxy(reinterpret_cast<char *>(key_instance->buf), key_instance->len, prompt);
}
static void release_secured_string(pybind11::memoryview string) {
  SecretStorageAccessor::release_secured_string(
    reinterpret_cast<char *>(PyMemoryView_GET_BUFFER(string.ptr())->buf)
  );
}

PYBIND11_MODULE(secret_storage_accessor, m) {
  m.doc() = "Accessor to secret-storage";
  m.def(
    "set_address",
    set_address,
    "set the path of socket to communicate with secret-storage server",
    pybind11::arg("socket_path")
  );
  m.def("ping", SecretStorageAccessor::ping, "check if a secret-storage server is up");
  m.def(
    "make_secure_key",
    make_secured_key,
    "make a key in locked memory, returns a secured string",
    pybind11::arg("length")
  );
  m.def(
    "get_secret",
    get_secret_plain,
    "get secret from storage, or ask for it and update to server. returns a secured string",
    pybind11::arg("key"),
    pybind11::arg("prompt").none(true)
  );
  m.def(
    "get_secret",
    get_secret_hidden,
    "get secret from storage, or ask for it and update to server. returns a secured string",
    pybind11::arg("key"),
    pybind11::arg("prompt").none(true)
  );
  m.def(
    "release_secure_string",
    release_secured_string,
    "free a secured string. you need this to avoid memory leakage.",
    pybind11::arg("string")
  );
}