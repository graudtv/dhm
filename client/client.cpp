#include <dhm/common.h>
#include <dhm/matrix.h>
#include <dhm/operation.h>
#include <dhm/protocol.h>

#include <boost/program_options.hpp>
#include <iostream>

using namespace dhm;
namespace po = boost::program_options;

po::options_description options("Options");

void showHelp() {
  std::cerr << "Usage: client [[--worker <url>]...]\n\n" << options << '\n';
  exit(1);
}

int main(int argc, char *argv[]) try {
  std::string operation_str = "echo";
  std::vector<std::string> worker_addrs;
  unsigned a_rows = 400, a_columns = 400, b_rows = 400, b_columns = 400;

  // clang-format off
  options.add_options()
    ("help,h", "Show help")
    ("show-data", "Print array data")
    ("worker,w", po::value(&worker_addrs), "Worker address ([host]:port). At least one worker must be specified")
    ("op", po::value(&operation_str), "Operation to perform.\nSupported opperations: 'echo', 'add'")
    ("ah", po::value(&a_rows), "Height of matrix A")
    ("aw", po::value(&a_columns), "Width of matrix A")
    ("bh", po::value(&b_rows), "Height of matrix B")
    ("bw", po::value(&b_columns), "Width of matrix B");
  // clang-format on
  po::parse_command_line(argc, argv, options);

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, options), vm);
  po::notify(vm);
  if (vm.count("help"))
    showHelp();
  if (!vm.count("worker")) {
    std::cerr << "Error: worker not specified\n\n";
    showHelp();
  }
  bool show_data = vm.count("show-data");
  Operation op = parseOperation(operation_str);

  boost::asio::io_context io_context;
  TcpCommunicationProtocol<float> protocol(io_context);

  for (auto &&addr : worker_addrs)
    protocol.addWorker(addr);

  if (op == OP_ECHO) {
    Echo echo(protocol);
    auto matrix = Matrix<float>::random(a_rows, a_columns);
    std::cout << "echo: matrix [" << matrix.rows() << " x " << matrix.columns()
              << "]" << std::endl;
    auto res = echo.echo(matrix);
    if (show_data) {
      print(matrix, "input");
      print(res, "result");
    }
    if (!std::equal(matrix.begin(), matrix.end(), res.begin()))
      throw std::runtime_error("echo: data mismatch!");
    std::cout << "echo: success!" << std::endl;
    return 0;
  }

  auto A = Matrix<float>::random(a_rows, a_columns);
  auto B = Matrix<float>::random(b_rows, b_columns);
  Matrix<float> res;
  Matrix<float> expected_res;

  std::cout << operation_str << ": matrix [" << A.rows() << " x " << A.columns()
            << "]" << std::endl;
  std::cout << operation_str << ": matrix [" << A.rows() << " x " << A.columns()
            << "]" << std::endl;

  if (op == OP_ADD) {
    if (a_rows != b_rows || a_columns != b_columns)
      throw std::runtime_error("error: incompatible matrix sizes");
    Adder adder(protocol);
    res = adder.add(A, B);
    expected_res = A + B;
  } else if (op == OP_MUL) {
    if (a_columns != b_rows)
      throw std::runtime_error("error: incompatible matrix sizes");
    Multiplier multiplier(protocol);
    res = multiplier.multiply(A, B);
    expected_res = A * B;
  } else {
    throw std::runtime_error("unsupported operation");
  }

  if (show_data) {
    print(A, "A");
    print(B, "B");
    print(res, "result");
    print(expected_res, "expected");
  }
  if (!std::equal(expected_res.begin(), expected_res.end(), res.begin()))
    throw std::runtime_error("add: incorrect result!");
  std::cout << operation_str << ": success!" << std::endl;
  return 0;
} catch (std::exception &e) {
  std::cerr << e.what() << std::endl;
}
