#include "secret_storage_accessor.hh"
#include <optional>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <utility>

static auto proxy(pybind11::memoryview view) -> std::string_view {
  auto buffer = PyMemoryView_GET_BUFFER(view.ptr());
  return {reinterpret_cast<char *>(buffer->buf), static_cast<size_t>(buffer->len)};
}

static auto proxy(std::string_view view) -> std::optional<pybind11::memoryview> {
  if (view.empty()) {
    return {};
  }
  return pybind11::memoryview::from_memory(
    const_cast<char *>(view.data()), static_cast<long>(view.size()), true
  );
}

PYBIND11_MODULE(secret_storage_accessor, m) {
  m.doc() = "Accessor to secret-storage";
  m.def(
    "release_secure_string",
    [](pybind11::memoryview string) -> void {
      SecretStorageAccessor::release_secured_string(
        {reinterpret_cast<char *>(PyMemoryView_GET_BUFFER(string.ptr())->buf), 1}
      );
    },
    "free a secured string. you need this to avoid memory leakage.",
    pybind11::arg("string")
  );
  m.def(
    "make_secured_key",
    [](size_t length) -> std::optional<pybind11::memoryview> {
      return proxy(SecretStorageAccessor::make_secured_key(length));
    },
    "make a randomly generated key that is stored in locked memory page",
    pybind11::arg("length")
  );
  m.def(
    "encode_string",
    [](pybind11::memoryview string) -> std::optional<pybind11::memoryview> {
      return proxy(SecretStorageAccessor::encode_string(proxy(std::move(string))));
    },
    "hex (base16) encode a string. yes, you have base64.b16encode, but this method encodes directly into the "
    "locked memory, that is what base64.b16encode cannot do.",
    pybind11::arg("string")
  );
  m.def(
    "ask_secret",
    [](const char *prompt, const char *retry_prompt = nullptr) -> std::optional<pybind11::memoryview> {
      return proxy(SecretStorageAccessor::ask_secret(prompt, retry_prompt));
    },
    "interactively ask user to enter a secret via stdin/stdout",
    pybind11::arg("prompt"),
    pybind11::arg("retry_prompt") = static_cast<const char *>(nullptr)
  );
  m.def(
    "set_socket_path",
    SecretStorageAccessor::set_socket_path,
    "set the path of socket used to communicate with secret storage server",
    pybind11::arg("socket_path") = static_cast<const char *>(nullptr)
  );
  m.def("ping", SecretStorageAccessor::ping, "check if a server is up and running");
  m.def(
    "exists",
    [](pybind11::memoryview key) -> bool { return SecretStorageAccessor::exists(proxy(std::move(key))); },
    "check if a secret is registered on the server",
    pybind11::arg("key")
  );
  m.def(
    "submit_secret",
    [](pybind11::memoryview key, pybind11::memoryview value, bool replace = false) -> bool {
      return SecretStorageAccessor::submit_secret(proxy(std::move(key)), proxy(std::move(value)), replace);
    },
    "set a secret directly to server",
    pybind11::arg("key"),
    pybind11::arg("value"),
    pybind11::kw_only(),
    pybind11::arg("replace") = false
  );
  m.def(
    "remove_secret",
    [](pybind11::memoryview key, bool allow_missing = false) -> bool {
      return SecretStorageAccessor::remove_secret(proxy(std::move(key)), allow_missing);
    },
    "delete a secret on server",
    pybind11::arg("key"),
    pybind11::kw_only(),
    pybind11::arg("allow_missing") = false
  );
  m.def("terminate_server", SecretStorageAccessor::terminate_server, "terminate the server");
  m.def(
    "get_secret",
    [](pybind11::memoryview key, const char *prompt = nullptr, bool update = true, bool remove = false)
      -> std::optional<pybind11::memoryview> {
      return proxy(SecretStorageAccessor::get_secret(
        proxy(std::move(key)), SecretStorageAccessor::GetOption().prompt(prompt).update(update).remove(remove)
      ));
    },
    "get a secret: try to retrieve the secret from server with the key. if that key does not exist on the "
    "server or server is down, ask user about it if prompt is not nullptr; if the secret is acquired by "
    "asking the user and update is true, submit the secret to server; if the secret is acquired from server "
    "and remove is true, remove it from server",
    pybind11::arg("key"),
    pybind11::kw_only(),
    pybind11::arg("prompt") = static_cast<const char *>(nullptr),
    pybind11::arg("update") = false,
    pybind11::arg("remove") = false
  );
  m.def(
    "ensure_secret",
    [](pybind11::memoryview key, const char *prompt) -> bool {
      return SecretStorageAccessor::ensure_secret(proxy(std::move(key)), prompt);
    },
    "ensure that a secret exists on the server. if the secret does not exist on the server, ask user for it "
    "and upload it",
    pybind11::arg("key"),
    pybind11::arg("prompt")
  );
}