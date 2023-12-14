#pragma once

#include "common.h"
#include "matrix.h"
#include <boost/asio.hpp>

namespace dhm {

/* Generic matrix distribution protocol */
template <class DataT> class CommunicationProtocol {
public:
  using data_type = DataT;

  virtual ~CommunicationProtocol(){};

  /* Ask worker_id to perform an operation */
  virtual void start(unsigned worker_id, Operation op) = 0;
  virtual void offload(unsigned worker_id, const DataT *data, unsigned rows,
                       unsigned columns) = 0;
  void offloadMatrix(unsigned worker_id, const Matrix<DataT> &matrix) {
    offload(worker_id, matrix.data(), matrix.rows(), matrix.columns());
  }

  /* Get result of the last offload to worker_id */
  virtual Matrix<DataT> waitResult(unsigned worker_id) = 0;

  /* Get number of available workers */
  virtual size_t getWorkerCount() const = 0;

  /* Low-level operations */
  virtual void sendRawData(unsigned worker_id, const void *data,
                           unsigned size) = 0;
  virtual void receiveRawData(unsigned worker_id, void *data,
                              unsigned size) = 0;

  void sendBuf(unsigned worker_id, const void *data, unsigned size) {
    sendRawData(worker_id, &size, sizeof(size));
    sendRawData(worker_id, data, size);
  }

  std::vector<char> receiveBuf(unsigned worker_id) {
    unsigned size = 0;
    receiveRawData(worker_id, &size, sizeof(size));
    std::vector<char> res(size);
    receiveRawData(worker_id, res.data(), size);
    return res;
  }
};

/* Raw tcp communication protocol */
template <class DataT>
class TcpCommunicationProtocol : public CommunicationProtocol<DataT> {
  boost::asio::io_context &io_context;
  tcp::resolver resolver;

  std::vector<tcp::socket> sockets;
  std::vector<std::vector<DataT>> results;

  std::unique_ptr<helib::Context> enc_context;

public:
  TcpCommunicationProtocol(boost::asio::io_context &ctx)
      : io_context(ctx), resolver(ctx) {}

  void addWorker(const std::string &addr);
  void start(unsigned worker_id, Operation op) override;
  void offload(unsigned worker_id, const DataT *data, unsigned rows,
               unsigned columns) override;
  Matrix<DataT> waitResult(unsigned worker_id) override;

  size_t getWorkerCount() const override { return sockets.size(); }

  void sendRawData(unsigned worker_id, const void *data,
                   unsigned size) override;
  void receiveRawData(unsigned worker_id, void *data, unsigned size) override;
};

/* Proxy class providing CKKS encryption on the top of another protocol */
class EncryptionProtocol : public CommunicationProtocol<double> {
  CommunicationProtocol *protocol;
  EncContextOptions context_options;
  helib::Context context;
  helib::SecKey sk;

public:
  EncryptionProtocol(CommunicationProtocol<double> *p,
                     const EncContextOptions &opts)
      : protocol(p), context_options(opts), context(opts.buildContext()),
        sk(context) {
    sk.GenSecKey();
    helib::addSome1DMatrices(sk);
  }

  const helib::PubKey &getPublicKey() { return sk; }
  const helib::SecKey &getSecretKey() { return sk; }

  void start(unsigned worker_id, Operation op) override {
    if (op == OP_ADD)
      op = OP_HADD;
    else if (op == OP_MUL)
      op = OP_HMUL;
    else
      throw std::runtime_error("unsupported operation for this protocol");
    protocol->start(worker_id, op);
    auto key = stringify(getPublicKey());
    auto worker_count = protocol->getWorkerCount();
    protocol->sendRawData(worker_id, &context_options, sizeof(context_options));
    protocol->sendBuf(worker_id, key.data(), key.size());
  }

  void offload(unsigned worker_id, const double *data, unsigned rows,
               unsigned columns) override {
    MatrixHeader hdr{rows, columns};
    protocol->sendRawData(worker_id, &hdr, sizeof(hdr));
    for (unsigned i = 0; i < rows; ++i) {
      const double *ptr = data + columns * i;
      std::vector<double> m(ptr, ptr + columns);
      auto enc = encrypt(m, getPublicKey());
      auto c = stringify(encrypt(m, getPublicKey()));
      protocol->sendBuf(worker_id, c.data(), c.size());
    }
  }

  Matrix<double> waitResult(unsigned worker_id) override {
    MatrixHeader hdr;
    protocol->receiveRawData(worker_id, &hdr, sizeof(hdr));
    std::vector<double> result;
    for (unsigned i = 0; i < hdr.rows(); ++i) {
      auto enc_row = readCtxt(getPublicKey(), protocol->receiveBuf(worker_id));
      auto row = decrypt(enc_row, getSecretKey());
      assert(row.size() == hdr.columns());
      result.insert(result.end(), row.begin(), row.end());
    }
    return Matrix<double>(std::move(result), hdr.columns());
  }

  size_t getWorkerCount() const override { return protocol->getWorkerCount(); }

  void sendRawData(unsigned worker_id, const void *data,
                   unsigned size) override {
    protocol->sendRawData(worker_id, data, size);
  }
  void receiveRawData(unsigned worker_id, void *data, unsigned size) override {
    protocol->sendRawData(worker_id, data, size);
  }
};

/* Parse "host:port" string */
inline std::pair<std::string, std::string> parseWorkerAddr(std::string Addr) {
  auto idx = Addr.find_last_of(':');
  if (idx == std::string::npos)
    throw std::runtime_error("Port not specified in URL '" + Addr + "'");
  std::string Host(Addr, 0, idx);
  std::string Port(Addr, idx + 1);
  if (Port.empty())
    throw std::runtime_error("Invalid port in URL '" + Addr + "'");
  if (Host.empty())
    Host = "localhost";
  return std::make_pair(Host, Port);
}

template <class DataT>
void TcpCommunicationProtocol<DataT>::addWorker(const std::string &addr) {
  auto [host, port] = parseWorkerAddr(addr);
  try {
    auto &socket = sockets.emplace_back(io_context);
    results.emplace_back();
    boost::asio::connect(socket, resolver.resolve(host, port));
  } catch (std::exception &e) {
    std::cout << "Error: '" << addr << "': " << e.what() << '\n';
    exit(1);
  }
}

template <class DataT>
void TcpCommunicationProtocol<DataT>::start(unsigned worker_id, Operation op) {
  send(op, sockets[worker_id]);
}

template <class DataT>
void TcpCommunicationProtocol<DataT>::offload(unsigned worker_id,
                                              const DataT *data, unsigned rows,
                                              unsigned columns) {
  MatrixHeader hdr(rows, columns);
  hdr.send(sockets[worker_id]);
  send_buf(data, rows * columns * sizeof(DataT), sockets[worker_id]);
}

template <class DataT>
Matrix<DataT> TcpCommunicationProtocol<DataT>::waitResult(unsigned worker_id) {
  auto hdr = MatrixHeader::receive(sockets[worker_id]);
  auto data =
      receive_buf<DataT>(hdr.rows() * hdr.columns(), sockets[worker_id]);
  return Matrix<DataT>(std::move(data), hdr.columns());
}

template <class DataT>
void TcpCommunicationProtocol<DataT>::sendRawData(unsigned worker_id,
                                                  const void *data,
                                                  unsigned size) {
  send_buf(data, size, sockets[worker_id]);
}

template <class DataT>
void TcpCommunicationProtocol<DataT>::receiveRawData(unsigned worker_id,
                                                     void *data,
                                                     unsigned size) {
  receive_buf(data, size, sockets[worker_id]);
}

} // namespace dhm
