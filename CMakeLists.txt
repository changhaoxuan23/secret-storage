cmake_minimum_required(VERSION 3.29)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_COMPILER clang++)

project(secret-storage VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# set compile options based on build type
set(CMAKE_CXX_FLAGS_DEBUG -g3)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -flto=full")
if(NOT DEFINED CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

option(MemoryAllocatorAlwaysFreePages
 "Whenever a memory block is freed, free the page it is on if the whole page is not allocated"
)

option(MemoryAllocatorWarnAboutPossibleLeakage
 "Try to generate a warning if a leakage is detected when closing the allocator"
)

add_compile_options(-fPIC -stdlib=libc++)
add_link_options(-fuse-ld=lld -stdlib=libc++)
add_compile_definitions(VERSION="${PROJECT_VERSION}")

find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
find_package(pybind11 CONFIG REQUIRED)

find_package(ConfigurationsPP REQUIRED)

add_executable(secret-storage
  main.cc
  server.cc
  message.cc
  storage.cc
  hardened_memory_allocator.cc
  command_line.cc
)

target_link_libraries(secret-storage PRIVATE ConfigurationsPP)

add_library(SecretStorageAccessor SHARED secret_storage_accessor.cc message.cc hardened_memory_allocator.cc)
target_compile_options(SecretStorageAccessor PUBLIC -stdlib=libc++)
target_link_options(SecretStorageAccessor PUBLIC -stdlib=libc++)
set_property(TARGET SecretStorageAccessor PROPERTY VERSION ${PROJECT_VERSION})
set_property(TARGET SecretStorageAccessor PROPERTY SOVERSION 1)
set_property(TARGET SecretStorageAccessor PROPERTY INTERFACE_SecretStorageAccessor_VERSION 1)
set_property(TARGET SecretStorageAccessor APPEND PROPERTY
  COMPATIBLE_INTERFACE_STRING INTERFACE_SecretStorageAccessor_VERSION
)
target_include_directories(SecretStorageAccessor INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include>
)

if(MemoryAllocatorAlwaysFreePages)
  target_compile_definitions(secret-storage PRIVATE MemoryAllocatorAlwaysFree)
  target_compile_definitions(SecretStorageAccessor PRIVATE MemoryAllocatorAlwaysFree)
endif()

if(MemoryAllocatorWarnAboutPossibleLeakage)
  target_compile_definitions(secret-storage PRIVATE MemoryAllocatorWarnLeakage)
  target_compile_definitions(SecretStorageAccessor PRIVATE MemoryAllocatorWarnLeakage)
endif()

install(TARGETS SecretStorageAccessor EXPORT SecretStorageAccessorTargets
  LIBRARY DESTINATION lib      COMPONENT ClientLibrary
  ARCHIVE DESTINATION lib      COMPONENT ClientLibrary
  RUNTIME DESTINATION bin      COMPONENT ClientLibrary
)
install(
  FILES
    secret_storage_accessor.hh
  DESTINATION
    include
  COMPONENT ClientLibrary
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/SecretStorageAccessor/SecretStorageAccessorConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY AnyNewerVersion
)

export(EXPORT SecretStorageAccessorTargets
  FILE "${CMAKE_CURRENT_BINARY_DIR}/SecretStorageAccessor/SecretStorageAccessorTargets.cmake"
)
configure_file(cmake/SecretStorageAccessorConfig.cmake
  "${CMAKE_CURRENT_BINARY_DIR}/SecretStorageAccessor/SecretStorageAccessorConfig.cmake"
  COPYONLY
)

set(ConfigPackageLocation lib/cmake/SecretStorageAccessor)
install(EXPORT SecretStorageAccessorTargets
  FILE
  SecretStorageAccessorTargets.cmake
  DESTINATION
    ${ConfigPackageLocation}
  COMPONENT ClientLibrary
)
install(
  FILES
    cmake/SecretStorageAccessorConfig.cmake
    "${CMAKE_CURRENT_BINARY_DIR}/SecretStorageAccessor/SecretStorageAccessorConfigVersion.cmake"
  DESTINATION
    ${ConfigPackageLocation}
  COMPONENT ClientLibrary
)

add_executable(secret-storage-ctl secret-storage-ctl.cc)
target_link_libraries(secret-storage-ctl PRIVATE ConfigurationsPP SecretStorageAccessor)
install(TARGETS secret-storage-ctl
  RUNTIME DESTINATION bin COMPONENT ClientLibrary
)


pybind11_add_module(secret_storage_accessor
  binding-python.cc
)
target_link_libraries(secret_storage_accessor PRIVATE SecretStorageAccessor)

install(TARGETS secret_storage_accessor
  DESTINATION lib/python${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR}/site-packages
  COMPONENT LanguageBinding
  COMPONENT PythonLanguageBinding
)
install(TARGETS secret-storage
  DESTINATION bin
  COMPONENT Server
)