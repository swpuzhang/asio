//
// detail/impl/descriptor_ops.ipp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2010 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_ASIO_DETAIL_IMPL_DESCRIPTOR_OPS_IPP
#define BOOST_ASIO_DETAIL_IMPL_DESCRIPTOR_OPS_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <boost/asio/detail/config.hpp>
#include <cerrno>
#include <boost/asio/detail/descriptor_ops.hpp>
#include <boost/asio/error.hpp>

#if !defined(BOOST_WINDOWS) && !defined(__CYGWIN__)

#include <boost/asio/detail/push_options.hpp>

namespace boost {
namespace asio {
namespace detail {
namespace descriptor_ops {

int open(const char* path, int flags, boost::system::error_code& ec)
{
  errno = 0;
  int result = error_wrapper(::open(path, flags), ec);
  if (result >= 0)
    ec = boost::system::error_code();
  return result;
}

int close(int d, state_type& state, boost::system::error_code& ec)
{
  int result = 0;
  if (d != -1)
  {
    if (state & internal_non_blocking)
    {
      ioctl_arg_type arg = 0;
      ::ioctl(d, FIONBIO, &arg);
      state &= ~internal_non_blocking;
    }

    errno = 0;
    result = error_wrapper(::close(d), ec);
  }

  if (result == 0)
    ec = boost::system::error_code();
  return result;
}

bool set_internal_non_blocking(int d,
    state_type& state, boost::system::error_code& ec)
{
  if (d == -1)
  {
    ec = boost::asio::error::bad_descriptor;
    return false;
  }

  errno = 0;
  ioctl_arg_type arg = 1;
  int result = error_wrapper(::ioctl(d, FIONBIO, &arg), ec);

  if (result >= 0)
  {
    ec = boost::system::error_code();
    state |= internal_non_blocking;
    return true;
  }

  return false;
}

std::size_t sync_read(int d, state_type state, buf* bufs,
    std::size_t count, bool all_empty, boost::system::error_code& ec)
{
  if (d == -1)
  {
    ec = boost::asio::error::bad_descriptor;
    return 0;
  }

  // A request to read 0 bytes on a stream is a no-op.
  if (all_empty)
  {
    ec = boost::system::error_code();
    return 0;
  }

  // Read some data.
  for (;;)
  {
    // Try to complete the operation without blocking.
    errno = 0;
    int bytes = error_wrapper(::readv(d, bufs, static_cast<int>(count)), ec);

    // Check if operation succeeded.
    if (bytes > 0)
      return bytes;

    // Check for EOF.
    if (bytes == 0)
    {
      ec = boost::asio::error::eof;
      return 0;
    }

    // Operation failed.
    if ((state & user_set_non_blocking)
        || (ec != boost::asio::error::would_block
          && ec != boost::asio::error::try_again))
      return 0;

    // Wait for descriptor to become ready.
    if (descriptor_ops::poll_read(d, ec) < 0)
      return 0;
  }
}

bool non_blocking_read(int d, buf* bufs, std::size_t count,
    boost::system::error_code& ec, std::size_t& bytes_transferred)
{
  for (;;)
  {
    // Read some data.
    errno = 0;
    int bytes = error_wrapper(::readv(d, bufs, static_cast<int>(count)), ec);

    // Check for end of stream.
    if (bytes == 0)
    {
      ec = boost::asio::error::eof;
      return 0;
    }

    // Retry operation if interrupted by signal.
    if (ec == boost::asio::error::interrupted)
      continue;

    // Check if we need to run the operation again.
    if (ec == boost::asio::error::would_block
        || ec == boost::asio::error::try_again)
      return false;

    // Operation is complete.
    if (bytes >= 0)
    {
      ec = boost::system::error_code();
      bytes_transferred = bytes;
    }
    else
      bytes_transferred = 0;

    return true;
  }
}

std::size_t sync_write(int d, state_type state, const buf* bufs,
    std::size_t count, bool all_empty, boost::system::error_code& ec)
{
  if (d == -1)
  {
    ec = boost::asio::error::bad_descriptor;
    return 0;
  }

  // A request to write 0 bytes on a stream is a no-op.
  if (all_empty)
  {
    ec = boost::system::error_code();
    return 0;
  }

  // Write some data.
  for (;;)
  {
    // Try to complete the operation without blocking.
    errno = 0;
    int bytes = error_wrapper(::writev(d, bufs, static_cast<int>(count)), ec);

    // Check if operation succeeded.
    if (bytes > 0)
      return bytes;

    // Operation failed.
    if ((state & user_set_non_blocking)
        || (ec != boost::asio::error::would_block
          && ec != boost::asio::error::try_again))
      return 0;

    // Wait for descriptor to become ready.
    if (descriptor_ops::poll_write(d, ec) < 0)
      return 0;
  }
}

bool non_blocking_write(int d, const buf* bufs, std::size_t count,
    boost::system::error_code& ec, std::size_t& bytes_transferred)
{
  for (;;)
  {
    // Write some data.
    errno = 0;
    int bytes = error_wrapper(::writev(d, bufs, static_cast<int>(count)), ec);

    // Retry operation if interrupted by signal.
    if (ec == boost::asio::error::interrupted)
      continue;

    // Check if we need to run the operation again.
    if (ec == boost::asio::error::would_block
        || ec == boost::asio::error::try_again)
      return false;

    // Operation is complete.
    if (bytes >= 0)
    {
      ec = boost::system::error_code();
      bytes_transferred = bytes;
    }
    else
      bytes_transferred = 0;

    return true;
  }
}

int ioctl(int d, state_type& state, long cmd,
    ioctl_arg_type* arg, boost::system::error_code& ec)
{
  if (d == -1)
  {
    ec = boost::asio::error::bad_descriptor;
    return -1;
  }

  errno = 0;
  int result = error_wrapper(::ioctl(d, cmd, arg), ec);

  if (result >= 0)
  {
    ec = boost::system::error_code();

    // When updating the non-blocking mode we always perform the ioctl syscall,
    // even if the flags would otherwise indicate that the descriptor is
    // already in the correct state. This ensures that the underlying
    // descriptor is put into the state that has been requested by the user. If
    // the ioctl syscall was successful then we need to update the flags to
    // match.
    if (cmd == static_cast<long>(FIONBIO))
    {
      if (*arg)
      {
        state |= user_set_non_blocking;
      }
      else
      {
        // Clearing the non-blocking mode always overrides any internally-set
        // non-blocking flag. Any subsequent asynchronous operations will need
        // to re-enable non-blocking I/O.
        state &= ~(user_set_non_blocking | internal_non_blocking);
      }
    }
  }

  return result;
}

int fcntl(int d, long cmd, boost::system::error_code& ec)
{
  if (d == -1)
  {
    ec = boost::asio::error::bad_descriptor;
    return -1;
  }

  errno = 0;
  int result = error_wrapper(::fcntl(d, cmd), ec);
  if (result != -1)
    ec = boost::system::error_code();
  return result;
}

int fcntl(int d, long cmd, long arg, boost::system::error_code& ec)
{
  if (d == -1)
  {
    ec = boost::asio::error::bad_descriptor;
    return -1;
  }

  errno = 0;
  int result = error_wrapper(::fcntl(d, cmd, arg), ec);
  if (result != -1)
    ec = boost::system::error_code();
  return result;
}

int poll_read(int d, boost::system::error_code& ec)
{
  if (d == -1)
  {
    ec = boost::asio::error::bad_descriptor;
    return -1;
  }

  pollfd fds;
  fds.fd = d;
  fds.events = POLLIN;
  fds.revents = 0;
  errno = 0;
  int result = error_wrapper(::poll(&fds, 1, -1), ec);
  if (result >= 0)
    ec = boost::system::error_code();
  return result;
}

int poll_write(int d, boost::system::error_code& ec)
{
  if (d == -1)
  {
    ec = boost::asio::error::bad_descriptor;
    return -1;
  }

  pollfd fds;
  fds.fd = d;
  fds.events = POLLOUT;
  fds.revents = 0;
  errno = 0;
  int result = error_wrapper(::poll(&fds, 1, -1), ec);
  if (result >= 0)
    ec = boost::system::error_code();
  return result;
}

} // namespace descriptor_ops
} // namespace detail
} // namespace asio
} // namespace boost

#include <boost/asio/detail/pop_options.hpp>

#endif // !defined(BOOST_WINDOWS) && !defined(__CYGWIN__)

#endif // BOOST_ASIO_DETAIL_IMPL_DESCRIPTOR_OPS_IPP
