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
  virtual void start(size_t worker_id, Operation op) = 0;
  virtual void offload(size_t worker_id, const DataT *data, unsigned rows,
                       unsigned columns) = 0;
  void offloadMatrix(size_t worker_id, const Matrix<DataT> &matrix) {
    offload(worker_id, matrix.data(), matrix.rows(), matrix.columns());
  }

  /* Get result of the last offload to worker_id */
  virtual Matrix<DataT> waitResult(size_t worker_id) = 0;

  /* Get number of available workers */
  virtual size_t getWorkerCount() const = 0;
};

/* Raw tcp communication protocol */
template <class DataT>
class TcpCommunicationProtocol : public CommunicationProtocol<DataT> {
  boost::asio::io_context &io_context;
  tcp::resolver resolver;

  std::vector<tcp::socket> sockets;
  std::vector<std::vector<DataT>> results;

public:
  TcpCommunicationProtocol(boost::asio::io_context &ctx)
      : io_context(ctx), resolver(ctx) {}

  void addWorker(const std::string &addr);
  void start(size_t worker_id, Operation op) override;
  void offload(size_t worker_id, const DataT *data, unsigned rows,
               unsigned columns) override;
  Matrix<DataT> waitResult(size_t worker_id) override;

  size_t getWorkerCount() const override { return sockets.size(); }
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
void TcpCommunicationProtocol<DataT>::start(size_t worker_id, Operation op) {
  send(op, sockets[worker_id]);
}

template <class DataT>
void TcpCommunicationProtocol<DataT>::offload(size_t worker_id,
                                              const DataT *data, unsigned rows,
                                              unsigned columns) {
  MatrixHeader hdr(rows, columns);
  hdr.send(sockets[worker_id]);
  send_buf(data, rows * columns * sizeof(DataT), sockets[worker_id]);
}

template <class DataT>
Matrix<DataT> TcpCommunicationProtocol<DataT>::waitResult(size_t worker_id) {
  auto hdr = MatrixHeader::receive(sockets[worker_id]);
  auto data =
      receive_buf<DataT>(hdr.rows() * hdr.columns(), sockets[worker_id]);
  return Matrix<DataT>(std::move(data), hdr.columns());
}

} // namespace dhm
