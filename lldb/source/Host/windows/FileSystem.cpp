//===-- FileSystem.cpp ------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "lldb/Host/windows/windows.h"

#include <shellapi.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "lldb/Host/FileSystem.h"
#include "lldb/Host/windows/AutoHandle.h"

#include "llvm/Support/ConvertUTF.h"
#include "llvm/Support/FileSystem.h"

using namespace lldb_private;

const char *FileSystem::DEV_NULL = "nul";

const char *FileSystem::PATH_CONVERSION_ERROR =
    "Error converting path between UTF-8 and native encoding";

Error FileSystem::Symlink(const FileSpec &src, const FileSpec &dst) {
  Error error;
  std::wstring wsrc, wdst;
  if (!llvm::ConvertUTF8toWide(src.GetCString(), wsrc) ||
      !llvm::ConvertUTF8toWide(dst.GetCString(), wdst))
    error.SetErrorString(PATH_CONVERSION_ERROR);
  if (error.Fail())
    return error;
  DWORD attrib = ::GetFileAttributesW(wdst.c_str());
  if (attrib == INVALID_FILE_ATTRIBUTES) {
    error.SetError(::GetLastError(), lldb::eErrorTypeWin32);
    return error;
  }
  bool is_directory = !!(attrib & FILE_ATTRIBUTE_DIRECTORY);
  DWORD flag = is_directory ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0;
  BOOL result = ::CreateSymbolicLinkW(wsrc.c_str(), wdst.c_str(), flag);
  if (!result)
    error.SetError(::GetLastError(), lldb::eErrorTypeWin32);
  return error;
}

Error FileSystem::Readlink(const FileSpec &src, FileSpec &dst) {
  Error error;
  std::wstring wsrc;
  if (!llvm::ConvertUTF8toWide(src.GetCString(), wsrc)) {
    error.SetErrorString(PATH_CONVERSION_ERROR);
    return error;
  }

  HANDLE h = ::CreateFileW(wsrc.c_str(), GENERIC_READ,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                           OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
  if (h == INVALID_HANDLE_VALUE) {
    error.SetError(::GetLastError(), lldb::eErrorTypeWin32);
    return error;
  }

  std::vector<wchar_t> buf(PATH_MAX + 1);
  // Subtract 1 from the path length since this function does not add a null
  // terminator.
  DWORD result = ::GetFinalPathNameByHandleW(
      h, buf.data(), buf.size() - 1, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
  std::string path;
  if (result == 0)
    error.SetError(::GetLastError(), lldb::eErrorTypeWin32);
  else if (!llvm::convertWideToUTF8(buf.data(), path))
    error.SetErrorString(PATH_CONVERSION_ERROR);
  else
    dst.SetFile(path, false);

  ::CloseHandle(h);
  return error;
}

Error FileSystem::ResolveSymbolicLink(const FileSpec &src, FileSpec &dst) {
  return Error("ResolveSymbolicLink() isn't implemented on Windows");
}

FILE *FileSystem::Fopen(const char *path, const char *mode) {
  std::wstring wpath, wmode;
  if (!llvm::ConvertUTF8toWide(path, wpath))
    return nullptr;
  if (!llvm::ConvertUTF8toWide(mode, wmode))
    return nullptr;
  FILE *file;
  if (_wfopen_s(&file, wpath.c_str(), wmode.c_str()) != 0)
    return nullptr;
  return file;
}
