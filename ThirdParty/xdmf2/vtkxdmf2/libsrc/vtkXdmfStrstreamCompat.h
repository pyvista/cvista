/*
 * vtkXdmfStrstreamCompat.h — <strstream> compatibility shim for vtkxdmf2.
 *
 * <strstream> was deprecated in C++98 and is unavailable on modern libc++
 * (Apple Clang), which broke the vendored xdmf2 build. Upstream VTK never
 * migrated xdmf2 off it. This header provides the exact subset of the legacy
 * interface xdmf2's libsrc uses, implemented over <sstream>, so the call sites
 * compile unchanged on every toolchain. It is intentionally minimal — only the
 * used members are modelled.
 *
 * Semantics preserved:
 *  - ostrstream default ctor: accumulating stream; str() returns a NUL-terminated
 *    char* buffer valid while the stream object is alive (every call site consumes
 *    or copies the pointer before the stream leaves scope).
 *  - ostrstream(buf, n): fixed-buffer mode; the accumulated text is written back
 *    into the caller's buffer (on str() and at destruction), matching the legacy
 *    behaviour of GetUnique()/GetHDFVersion()/AddArrayToList().
 *  - istrstream(buf, n): read stream over the first n chars of buf.
 *  - freeze(int): no-op (this shim never hands out an unmanaged buffer).
 * std::ends is left untouched: it writes a trailing '\0' which is harmless under
 * the c_str()/char* consumption every call site performs.
 */
#ifndef vtkXdmfStrstreamCompat_h
#define vtkXdmfStrstreamCompat_h

#include <algorithm>
#include <cstring>
#include <sstream>
#include <string>

namespace xdmf2compat
{

class ostrstream : public std::ostringstream
{
  char* Ext = nullptr;
  std::streamsize ExtN = 0;
  std::string Frozen;

  void SyncExt()
  {
    if (this->Ext && this->ExtN > 0)
    {
      const std::string s = this->std::ostringstream::str();
      const std::streamsize n =
        std::min<std::streamsize>(static_cast<std::streamsize>(s.size()), this->ExtN - 1);
      std::memcpy(this->Ext, s.data(), static_cast<std::size_t>(n));
      this->Ext[n] = '\0';
    }
  }

public:
  ostrstream() = default;
  ostrstream(char* buf, std::streamsize n)
    : Ext(buf)
    , ExtN(n)
  {
  }
  ~ostrstream() override { this->SyncExt(); }

  // Returns a NUL-terminated char* valid while this object is alive (fixed-buffer
  // mode returns the caller's buffer). Matches the legacy ostrstream::str() type.
  char* str()
  {
    if (this->Ext)
    {
      this->SyncExt();
      return this->Ext;
    }
    this->Frozen = this->std::ostringstream::str();
    return &this->Frozen[0];
  }

  void freeze(int = 1) {}
};

class istrstream : public std::istringstream
{
public:
  istrstream(const char* s, std::streamsize n)
    : std::istringstream(std::string(s ? s : "", s ? static_cast<std::size_t>(n) : 0))
  {
  }
};

// Bidirectional strstream is only named in a `using` declaration, never
// instantiated; provide a minimal stand-in for completeness.
class strstream : public std::stringstream
{
public:
  char* str()
  {
    this->Frozen = this->std::stringstream::str();
    return &this->Frozen[0];
  }
  void freeze(int = 1) {}

private:
  std::string Frozen;
};

} // namespace xdmf2compat

#endif // vtkXdmfStrstreamCompat_h
