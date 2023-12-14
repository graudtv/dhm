#pragma once

#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <helib/helib.h>
#include <iostream>

namespace dhm {

using boost::asio::ip::tcp;

template <class DataT> DataT receive(tcp::socket &socket) {
  DataT value;
  auto sz = socket.receive(boost::asio::buffer(&value, sizeof value));
  assert(sz == sizeof value);
  return value;
}

/* receive value unless eof reached */
template <class DataT> bool try_receive(DataT &value, tcp::socket &socket) try {
  auto sz = socket.receive(boost::asio::buffer(&value, sizeof value));
  assert(sz == sizeof value);
  return true;
} catch (boost::system::system_error &e) {
  if (e.code() == boost::asio::error::eof)
    return false;
  throw e;
}

template <class DataT> void send(const DataT &value, tcp::socket &socket) {
  socket.send(boost::asio::buffer(&value, sizeof value));
}

inline void send_buf(const void *data, unsigned size, tcp::socket &socket) {
  const char *ptr = (const char *)data;
  while (size > 0) {
    size_t chunk_sz = socket.send(boost::asio::buffer(ptr, size));
    size -= chunk_sz;
    ptr += chunk_sz;
  }
}

inline void receive_buf(void *data, unsigned size, tcp::socket &socket) {
  char *ptr = (char *)data;
  while (size > 0) {
    auto chunk_sz = socket.receive(boost::asio::buffer(ptr, size));
    size -= chunk_sz;
    ptr += chunk_sz;
  }
}

template <class DataT>
void send_buf(const std::vector<DataT> &data, tcp::socket &socket) {
  send_buf(data.data(), data.size() * sizeof(DataT), socket);
}

template <class DataT>
std::vector<DataT> receive_buf(unsigned size, tcp::socket &socket) {
  std::vector<DataT> data(size);
  receive_buf(data.data(), size * sizeof(DataT), socket);
  return data;
}

enum Operation : unsigned { OP_ECHO, OP_ADD, OP_MUL, OP_HADD, OP_HMUL };

inline const char *opToString(Operation op) {
  switch (op) {
  case OP_ECHO:
    return "echo";
  case OP_ADD:
    return "add";
  case OP_MUL:
    return "mul";
  case OP_HADD:
    return "hadd";
  case OP_HMUL:
    return "hmul";
  default:
    return "<invalid_operation>";
  }
}

inline Operation parseOperation(const std::string &op) {
  if (op == "echo")
    return OP_ECHO;
  if (op == "add")
    return OP_ADD;
  if (op == "mul")
    return OP_MUL;
  if (op == "hadd")
    return OP_HADD;
  if (op == "hmul")
    return OP_HMUL;
  throw std::runtime_error("invalid operation '" + op + "'");
}

struct MatrixHeader {
  std::array<unsigned, 2> data;
  unsigned &rows() { return data[0]; }
  unsigned &columns() { return data[1]; }

  MatrixHeader() = default;
  MatrixHeader(unsigned r, unsigned c) {
    data[0] = r;
    data[1] = c;
  }

  void send(tcp::socket &socket) {
    auto sz = socket.send(boost::asio::buffer(data));
  }
  static MatrixHeader receive(tcp::socket &socket) {
    MatrixHeader hdr;
    size_t sz = 0;
    sz = socket.receive(boost::asio::buffer(hdr.data));
    assert(sz == sizeof(unsigned) * 2);
    return hdr;
  }
};

struct EncContextOptions {
  unsigned m;
  unsigned bits;
  unsigned precision;
  unsigned c;

  EncContextOptions() = default;
  EncContextOptions(unsigned m, unsigned bits, unsigned precision, unsigned c)
      : m(m), bits(bits), precision(precision), c(c) {}

  helib::Context buildContext() const {
    return helib::ContextBuilder<helib::CKKS>()
        .m(m)
        .bits(bits)
        .precision(precision)
        .c(c)
        .build();
  }
};

inline std::string stringify(const helib::Ctxt &c) {
  std::ostringstream os;
  c.writeTo(os);
  return os.str();
}

inline std::string stringify(const helib::PubKey &pk) {
  std::ostringstream os;
  pk.writeTo(os);
  return os.str();
}

inline helib::Ctxt encrypt(const std::vector<double> &data,
                           const helib::PubKey &pk) {
  helib::PtxtArray m(pk.getContext(), data);
  helib::Ctxt c(pk);
  m.encrypt(c);
  return c;
}

inline std::vector<double> decrypt(const helib::Ctxt &c, const helib::SecKey &sk) {
  helib::PtxtArray res(sk.getContext());
  res.decrypt(c, sk);
  std::vector<double> vres;
  res.store(vres);
  return vres;
}

inline helib::Ctxt readCtxt(const helib::PubKey &pk, const std::vector<char> &text) {
  std::string str(text.begin(), text.end());
  std::istringstream is(str);
  return helib::Ctxt::readFrom(is, pk);
}

inline helib::Ctxt readCtxt(const helib::PubKey &pk, const std::string &text) {
  std::istringstream is(text);
  return helib::Ctxt::readFrom(is, pk);
}

inline helib::PubKey readKey(const helib::Context &ctx,
                             const std::string &text) {
  std::istringstream is(text);
  return helib::PubKey::readFrom(is, ctx);
}

} // namespace dhm
