/*
 * University of Illinois/NCSA
 * Open Source License
 * 
 * Copyright (c) 2003-2013 University of Illinois at Urbana-Champaign.
 * All rights reserved.
 * 
 * Developed by:
 *     LLVM Team
 *     University of Illinois at Urbana-Champaign
 *     http://llvm.org
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal with
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 * 
 *     * Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimers.
 * 
 *     * Redistributions in binary form must reproduce the above copyright notice,
 *       this list of conditions and the following disclaimers in the
 *       documentation and/or other materials provided with the distribution.
 * 
 *     * Neither the names of the LLVM Team, University of Illinois at
 *       Urbana-Champaign, nor the names of its contributors may be used to
 *       endorse or promote products derived from this Software without specific
 *       prior written permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS WITH THE
 * SOFTWARE.
 */

#include "sippet/base/raw_ostream.h"

#include <sys/stat.h>
#include <sys/types.h>

#include <cctype>
#include <cerrno>
#include <algorithm>
#include <vector>

#include "sippet/base/format.h"
#include "sippet/base/stl_extras.h"
#include "sippet/base/string_extras.h"

namespace sippet {

raw_ostream::~raw_ostream() {
  // raw_ostream's subclasses should take care to flush the buffer
  // in their destructors.
  assert(OutBufCur == OutBufStart &&
         "raw_ostream destructor called with non-empty buffer!");

  if (BufferMode == InternalBuffer)
    delete [] OutBufStart;
}

// An out of line virtual method to provide a home for the class vtable.
void raw_ostream::handle() {}

size_t raw_ostream::preferred_buffer_size() const {
  // BUFSIZ is intended to be a reasonable default.
  return BUFSIZ;
}

void raw_ostream::SetBuffered() {
  // Ask the subclass to determine an appropriate buffer size.
  if (size_t Size = preferred_buffer_size())
    SetBufferSize(Size);
  else
    // It may return 0, meaning this stream should be unbuffered.
    SetUnbuffered();
}

void raw_ostream::SetBufferAndMode(char *BufferStart, size_t Size,
                                   BufferKind Mode) {
  assert(((Mode == Unbuffered && BufferStart == 0 && Size == 0) ||
          (Mode != Unbuffered && BufferStart && Size)) &&
         "stream must be unbuffered or have at least one byte");
  // Make sure the current buffer is free of content (we can't flush here; the
  // child buffer management logic will be in write_impl).
  assert(GetNumBytesInBuffer() == 0 && "Current buffer is non-empty!");

  if (BufferMode == InternalBuffer)
    delete [] OutBufStart;
  OutBufStart = BufferStart;
  OutBufEnd = OutBufStart+Size;
  OutBufCur = OutBufStart;
  BufferMode = Mode;

  assert(OutBufStart <= OutBufEnd && "Invalid size!");
}

raw_ostream &raw_ostream::operator<<(uint32 N) {
  // Zero is a special case.
  if (N == 0)
    return *this << '0';

  char NumberBuffer[20];
  char *EndPtr = NumberBuffer+sizeof(NumberBuffer);
  char *CurPtr = EndPtr;

  while (N) {
    *--CurPtr = '0' + static_cast<char>(N % 10);
    N /= 10;
  }
  return write(CurPtr, EndPtr-CurPtr);
}

raw_ostream &raw_ostream::operator<<(int32 N) {
  if (N <  0) {
    *this << '-';
    // Avoid undefined behavior on LONG_MIN with a cast.
    N = static_cast<uint32>(-N);
  }

  return this->operator<<(static_cast<uint32>(N));
}

raw_ostream &raw_ostream::operator<<(uint64 N) {
  // Output using 32-bit div/mod when possible.
  if (N == static_cast<uint32>(N))
    return this->operator<<(static_cast<uint32>(N));

  char NumberBuffer[20];
  char *EndPtr = NumberBuffer+sizeof(NumberBuffer);
  char *CurPtr = EndPtr;

  while (N) {
    *--CurPtr = '0' + static_cast<char>(N % 10);
    N /= 10;
  }
  return write(CurPtr, EndPtr-CurPtr);
}

raw_ostream &raw_ostream::operator<<(int64 N) {
  if (N < 0) {
    *this << '-';
    // Avoid undefined behavior on INT64_MIN with a cast.
    N = static_cast<uint64>(-N);
  }

  return this->operator<<(static_cast<uint64>(N));
}

raw_ostream &raw_ostream::write_hex(uint64 N) {
  // Zero is a special case.
  if (N == 0)
    return *this << '0';

  char NumberBuffer[20];
  char *EndPtr = NumberBuffer+sizeof(NumberBuffer);
  char *CurPtr = EndPtr;

  while (N) {
    uintptr_t x = N % 16;
    *--CurPtr = (x < 10 ? '0' + x : 'a' + x - 10);
    N /= 16;
  }

  return write(CurPtr, EndPtr-CurPtr);
}

raw_ostream &raw_ostream::write_escaped(const base::StringPiece &Str,
                                        bool UseHexEscapes) {
  for (size_t i = 0, e = Str.size(); i != e; ++i) {
    unsigned char c = Str[i];

    switch (c) {
    case '\\':
      *this << '\\' << '\\';
      break;
    case '\t':
      *this << '\\' << 't';
      break;
    case '\n':
      *this << '\\' << 'n';
      break;
    case '"':
      *this << '\\' << '"';
      break;
    default:
      if (std::isprint(c)) {
        *this << c;
        break;
      }

      // Write out the escaped representation.
      if (UseHexEscapes) {
        *this << '\\' << 'x';
        *this << hexdigit((c >> 4 & 0xF));
        *this << hexdigit((c >> 0) & 0xF);
      } else {
        // Always use a full 3-character octal escape.
        *this << '\\';
        *this << char('0' + ((c >> 6) & 7));
        *this << char('0' + ((c >> 3) & 7));
        *this << char('0' + ((c >> 0) & 7));
      }
    }
  }

  return *this;
}

raw_ostream &raw_ostream::operator<<(double N) {
#ifdef _WIN32
  // On MSVCRT and compatible, output of %e is incompatible to Posix
  // by default. Number of exponent digits should be at least 2. "%+03d"
  // FIXME: Implement our formatter to here or Support/Format.h!
  int fpcl = _fpclass(N);

  // negative zero
  if (fpcl == _FPCLASS_NZ)
    return *this << "-0.000000e+00";

  char buf[16];
  unsigned len;
  len = base::snprintf(buf, sizeof(buf), "%e", N);
  if (len <= sizeof(buf) - 2) {
    if (len >= 5 && buf[len - 5] == 'e' && buf[len - 3] == '0') {
      int cs = buf[len - 4];
      if (cs == '+' || cs == '-') {
        int c1 = buf[len - 2];
        int c0 = buf[len - 1];
        if (isdigit(static_cast<unsigned char>(c1)) &&
            isdigit(static_cast<unsigned char>(c0))) {
          // Trim leading '0': "...e+012" -> "...e+12\0"
          buf[len - 3] = c1;
          buf[len - 2] = c0;
          buf[--len] = 0;
        }
      }
    }
    return this->operator<<(buf);
  }
#endif
  return this->operator<<(format("%e", N));
}

void raw_ostream::flush_nonempty() {
  assert(OutBufCur > OutBufStart && "Invalid call to flush_nonempty.");
  size_t Length = OutBufCur - OutBufStart;
  OutBufCur = OutBufStart;
  write_impl(OutBufStart, Length);
}

raw_ostream &raw_ostream::write(unsigned char C) {
  // Group exceptional cases into a single branch.
  if (OutBufCur >= OutBufEnd) {
    if (!OutBufStart) {
      if (BufferMode == Unbuffered) {
        write_impl(reinterpret_cast<char*>(&C), 1);
        return *this;
      }
      // Set up a buffer and start over.
      SetBuffered();
      return write(C);
    }

    flush_nonempty();
  }

  *OutBufCur++ = C;
  return *this;
}

raw_ostream &raw_ostream::write(const char *Ptr, size_t Size) {
  // Group exceptional cases into a single branch.
  if (size_t(OutBufEnd - OutBufCur) < Size) {
    if (!OutBufStart) {
      if (BufferMode == Unbuffered) {
        write_impl(Ptr, Size);
        return *this;
      }
      // Set up a buffer and start over.
      SetBuffered();
      return write(Ptr, Size);
    }

    size_t NumBytes = OutBufEnd - OutBufCur;

    // If the buffer is empty at this point we have a string that is larger
    // than the buffer. Directly write the chunk that is a multiple of the
    // preferred buffer size and put the remainder in the buffer.
    if (OutBufCur == OutBufStart) {
      size_t BytesToWrite = Size - (Size % NumBytes);
      write_impl(Ptr, BytesToWrite);
      size_t BytesRemaining = Size - BytesToWrite;
      if (BytesRemaining > size_t(OutBufEnd - OutBufCur)) {
        // Too much left over to copy into our buffer.
        return write(Ptr + BytesToWrite, BytesRemaining);
      }
      copy_to_buffer(Ptr + BytesToWrite, BytesRemaining);
      return *this;
    }

    // We don't have enough space in the buffer to fit the string in. Insert as
    // much as possible, flush and start over with the remainder.
    copy_to_buffer(Ptr, NumBytes);
    flush_nonempty();
    return write(Ptr + NumBytes, Size - NumBytes);
  }

  copy_to_buffer(Ptr, Size);

  return *this;
}

void raw_ostream::copy_to_buffer(const char *Ptr, size_t Size) {
  assert(Size <= size_t(OutBufEnd - OutBufCur) && "Buffer overrun!");

  // Handle short strings specially, memcpy isn't very good at very short
  // strings.
  switch (Size) {
  case 4: OutBufCur[3] = Ptr[3];  // FALL THROUGH
  case 3: OutBufCur[2] = Ptr[2];  // FALL THROUGH
  case 2: OutBufCur[1] = Ptr[1];  // FALL THROUGH
  case 1: OutBufCur[0] = Ptr[0];  // FALL THROUGH
  case 0: break;
  default:
    memcpy(OutBufCur, Ptr, Size);
    break;
  }

  OutBufCur += Size;
}

// Formatted output.
raw_ostream &raw_ostream::operator<<(const format_object_base &Fmt) {
  // If we have more than a few bytes left in our output buffer, try
  // formatting directly onto its end.
  size_t NextBufferSize = 127;
  size_t BufferBytesLeft = OutBufEnd - OutBufCur;
  if (BufferBytesLeft > 3) {
    size_t BytesUsed = Fmt.print(OutBufCur, BufferBytesLeft);

    // Common case is that we have plenty of space.
    if (BytesUsed <= BufferBytesLeft) {
      OutBufCur += BytesUsed;
      return *this;
    }

    // Otherwise, we overflowed and the return value tells us the size to try
    // again with.
    NextBufferSize = BytesUsed;
  }

  // If we got here, we didn't have enough space in the output buffer for the
  // string. Try printing into a vector that is resized to have enough
  // space. Iterate until we win.
  std::vector<char> V;

  while (1) {
    V.resize(NextBufferSize);

    // Try formatting into the SmallVector.
    size_t BytesUsed = Fmt.print(&V.front(), NextBufferSize);

    // If BytesUsed fit into the vector, we win.
    if (BytesUsed <= NextBufferSize)
      return write(&V.front(), BytesUsed);

    // Otherwise, try again with a new size.
    assert(BytesUsed > NextBufferSize && "Didn't grow buffer!?");
    NextBufferSize = BytesUsed;
  }
}

/// indent - Insert 'NumSpaces' spaces.
raw_ostream &raw_ostream::indent(size_t NumSpaces) {
  static const char Spaces[] = "                                "
                               "                                "
                               "                ";

  // Usually the indentation is small, handle it with a fastpath.
  if (NumSpaces < array_lengthof(Spaces))
    return write(Spaces, NumSpaces);

  while (NumSpaces) {
    size_t NumToWrite = std::min(NumSpaces,
        (size_t)array_lengthof(Spaces)-1);
    write(Spaces, NumToWrite);
    NumSpaces -= NumToWrite;
  }
  return *this;
}


//===----------------------------------------------------------------------===//
//  Formatted Output
//===----------------------------------------------------------------------===//

// Out of line virtual method.
void format_object_base::home() {
}

//===----------------------------------------------------------------------===//
//  raw_string_ostream
//===----------------------------------------------------------------------===//

raw_string_ostream::~raw_string_ostream() {
  flush();
}

uint64 raw_string_ostream::current_pos() const {
  return OS.size();
}

void raw_string_ostream::write_impl(const char *Ptr, size_t Size) {
  OS.append(Ptr, Size);
}

}  // namespace sippet
