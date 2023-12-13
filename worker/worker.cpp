#include <dhm/common.h>
#include <dhm/matrix.h>

#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <ctime>
#include <iostream>
#include <string>
#include <thread>

using namespace dhm;

#define DBG 0

class TcpConnection : public boost::enable_shared_from_this<TcpConnection> {
  tcp::socket socket;

public:
  using pointer = boost::shared_ptr<TcpConnection>;

  static pointer create(boost::asio::io_context &io_context) {
    return pointer(new TcpConnection(io_context));
  }

  tcp::socket &getSocket() { return socket; }

  void start() {
    std::cerr << "> " << socket.remote_endpoint() << ": session started"
              << std::endl;
    waitRequest();
  }

  void waitRequest() {
    socket.async_wait(
        tcp::socket::wait_read,
        boost::bind(&TcpConnection::handleRequest, shared_from_this()));
  }

  ~TcpConnection() {
    std::cerr << "> " << socket.remote_endpoint() << ": session ended"
              << std::endl;
  }

private:
  TcpConnection(boost::asio::io_context &io_context) : socket(io_context) {}

  void handleRequest() try {
    Operation op;
    if (!try_receive(op, socket))
      return;

    std::cerr << "> " << socket.remote_endpoint()
              << ": request: " << opToString(op) << std::endl;
    if (op == OP_ECHO)
      handleEcho<float>();
    else if (op == OP_ADD || op == OP_MUL)
      handleBinOp<float>(op);
    else
      throw std::runtime_error("unsupported operation");
    std::cerr << "> " << socket.remote_endpoint() << ": sent result"
              << std::endl;
    waitRequest();
  } catch (std::exception &e) {
    std::cerr << "> " << socket.remote_endpoint() << ": " << e.what()
              << std::endl;
  }

  template <class T> void handleEcho();
  template <class T> void handleBinOp(Operation op);
};

class TcpServer {
public:
  TcpServer(boost::asio::io_context &io_context, unsigned port)
      : context(io_context),
        acceptor(io_context, tcp::endpoint(tcp::v4(), port)) {
    std::cout << "> listening on port " << port << std::endl;
    startAccept();
  }

private:
  void startAccept() {
    auto connection = TcpConnection::create(context);
    acceptor.async_accept(connection->getSocket(),
                          boost::bind(&TcpServer::handleAccept, this,
                                      connection,
                                      boost::asio::placeholders::error));
  }

  void handleAccept(TcpConnection::pointer connection,
                    const boost::system::error_code &error) {
    if (!error) {
      connection->start();
    }
    startAccept();
  }

  boost::asio::io_context &context;
  tcp::acceptor acceptor;
};

template <class DataT> void TcpConnection::handleEcho() {
  auto hdr = MatrixHeader::receive(socket);
  auto data = receive_buf<DataT>(hdr.rows() * hdr.columns(), socket);
  std::cout << "> " << socket.remote_endpoint() << ": received matrix ["
            << hdr.rows() << " x " << hdr.columns() << "]" << std::endl;
  hdr.send(socket);
  send_buf(data, socket);
}

template <class DataT> void TcpConnection::handleBinOp(Operation op) {
  auto hdr1 = MatrixHeader::receive(socket);
  auto data1 = receive_buf<DataT>(hdr1.rows() * hdr1.columns(), socket);
  std::cout << "> " << socket.remote_endpoint() << ": received matrix ["
            << hdr1.rows() << " x " << hdr1.columns() << "]" << std::endl;
  auto hdr2 = MatrixHeader::receive(socket);
  auto data2 = receive_buf<DataT>(hdr2.rows() * hdr2.columns(), socket);
  std::cout << "> " << socket.remote_endpoint() << ": received matrix ["
            << hdr2.rows() << " x " << hdr2.columns() << "]" << std::endl;
  Matrix<DataT> A(data1, hdr1.columns());
  Matrix<DataT> B(data2, hdr2.columns());
#if DBG
  print(A, "A");
  print(B, "B");
#endif
  if (op == OP_ADD) {
    if (hdr1.rows() != hdr2.rows() || hdr1.columns() != hdr2.columns())
      throw std::runtime_error("mismatching matrix sizes");
    A += B;
  } else if (op == OP_MUL) {
    A = mulT(A, B);
  } else {
    throw std::runtime_error("unsupported operation");
  }
  hdr1.send(socket);
  send_buf(A.data(), A.size() * sizeof(DataT), socket);
}

int main(int argc, char *argv[]) try {
  if (argc != 2) {
    std::cerr << "Usage: ./worker <port>" << std::endl;
    exit(1);
  }
  boost::asio::io_context io_context;
  int port = atoi(argv[1]);
  TcpServer server(io_context, port);
  io_context.run();
  return 0;
} catch (std::exception &E) {
  std::cerr << E.what() << std::endl;
}
