#ifndef STORAGE_HH_
#define STORAGE_HH_
#include <memory>
class Storage {
private:
  std::unique_ptr<void> implementation;

public:
};
#endif