#ifndef MESSAGE_HH_
#define MESSAGE_HH_
#include <cstdint>
#include <optional>
#include <sys/un.h>

struct Message {
  enum Type : uint8_t {
    Ping, // client -> server, checks if a server is running
          //  flags: none, reserved, set to 0
          //  argument: SingleEntryBody with arbitrary data(nonce) of any length
          //  reply: a Pong message with nonce repeated (echoed)

    Pong, // server -> client, reply to Ping

    Add, // client -> server, add a new secret into storage
         //  flags: Add_ReplaceExisting
         //         Add_OneTimeUse
         //  argument: DoubleEntryBody of key and value
         //  reply: an Ok message if succeed, or Failed message otherwise

    Query, // client -> server, query some secret
           //  flags: Query_ExistenceOnly
           //  argument: SingleEntryBody of key
           //  reply: a Result message with the value or Failed message

    Delete, // client -> server, remove some secret from storage
            //  flags: Delete_AllowMissing
            //  argument: SingleEntryBody of key
            //  reply: an Ok message if succeed, or Failed message otherwise

    Ok, // server -> client, no content reply indicating a positive result, succeed or true

    Failed, // server -> client, indicates a negative result, failed or false
            //  flags: Failed_DescriptionAttached

    Result, // server -> client, result of some query with a SingleEntryBody

    Terminate, // client -> server, ask the server to terminate
               //  flags: none, reserved, set to 0
               //  argument: none
               //  reply: none
  } type;
  enum Flags : uint8_t {
    Add_ReplaceExisting = 0x1, // replace corresponding value if the key exists
                               //  an Add operation shall fail by default if the key already exists

    Query_ExistenceOnly = 0x1, // only check if the secret exists and reply with Ok/Failed
                               //  do not retrieve value

    Query_DeleteSecret = 0x2, // delete the secret after query

    Delete_AllowMissing = 0x1, // treat missing key as deleted successfully instead of a failure

    Failed_DescriptionAttached = 0x1, // this Failed massage has a SingleEntryBody with a string
                                      //  which indicates the cause of failure
  };
  uint8_t flags;
  uint8_t data[];
};
struct SingleEntryBody {
  uint16_t length;
  uint8_t  data[];
  void     receive(int socket_fd);
};
struct DoubleEntryBody {
  uint16_t length[2];
  uint8_t  data[];
  void     receive(int socket_fd);
};

auto make_address(const char *path = nullptr, bool create = false) -> std::optional<sockaddr_un>;
#endif