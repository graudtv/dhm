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

template <class R1, class R2>
bool equal(R1 &&Range1, R2 &&Range2, float eps = 0.001) {
  return std::equal(std::begin(Range1), std::end(Range1), std::begin(Range2),
                    std::end(Range2), [eps](auto &&Fst, auto &&Snd) {
                      return std::fabs(Fst - Snd) < eps;
                    });
}

template <class R1, class R2> double dist(R1 &&Range1, R2 &&Range2) {
  auto It1 = std::begin(Range1), Ite = std::end(Range1);
  auto It2 = std::begin(Range2);
  double dist = 0;
  while (It1 != Ite)
    dist += std::fabs(*It1++ - *It2++);
  return dist;
}

int main(int argc, char *argv[]) try {
  std::string operation_str = "echo";
  std::vector<std::string> worker_addrs;
  unsigned a_rows = 512, a_columns = 512, b_rows = 512, b_columns = 512;
  unsigned common_size = 0;

  // clang-format off
  options.add_options()
    ("help,h", "Show help")
    ("show-data", "Print array data")
    ("worker,w", po::value(&worker_addrs), "Worker address ([host]:port). At least one worker must be specified")
    ("op", po::value(&operation_str), "Operation to perform.\nSupported opperations: 'echo', 'add'")
    ("ah", po::value(&a_rows), "Height of matrix A")
    ("aw", po::value(&a_columns), "Width of matrix A")
    ("bh", po::value(&b_rows), "Height of matrix B")
    ("bw", po::value(&b_columns), "Width of matrix B")
    ("size", po::value(&common_size), "Set all sizes to the same value. Overrides ah, aw, bh, bw");
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
  if (vm.count("size"))
    a_rows = a_columns = b_rows = b_columns = common_size;

  bool show_data = vm.count("show-data");
  Operation op = parseOperation(operation_str);

  boost::asio::io_context io_context;
  TcpCommunicationProtocol<double> tcp_protocol(io_context);
  std::unique_ptr<EncryptionProtocol> enc_protocol;
  CommunicationProtocol<double> *protocol = &tcp_protocol;

  if (op == OP_HADD || op == OP_HMUL) {
    EncContextOptions opts(4 * a_columns, 119, 20, 2);
    enc_protocol = std::make_unique<EncryptionProtocol>(&tcp_protocol, opts);
    protocol = enc_protocol.get();
  }

  for (auto &&addr : worker_addrs)
    tcp_protocol.addWorker(addr);

  if (op == OP_ECHO) {
    Echo echo(tcp_protocol);
    auto matrix = Matrix<double>::random(a_rows, a_columns);
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

  auto A = Matrix<double>::random(a_rows, a_columns);
  auto B = Matrix<double>::random(b_rows, b_columns);
  Matrix<double> res;
  Matrix<double> expected_res;

  std::cout << operation_str << ": matrix [" << A.rows() << " x " << A.columns()
            << "]" << std::endl;
  std::cout << operation_str << ": matrix [" << A.rows() << " x " << A.columns()
            << "]" << std::endl;

  if (op == OP_ADD || op == OP_HADD) {
    if (a_rows != b_rows || a_columns != b_columns)
      throw std::runtime_error("error: incompatible matrix sizes");
    Adder adder(*protocol);
    res = adder.add(A, B);
    expected_res = A + B;
  } else if (op == OP_MUL || op == OP_HMUL) {
    if (a_columns != b_rows)
      throw std::runtime_error("error: incompatible matrix sizes");
    if (op == OP_HMUL && (a_rows != a_columns || b_rows != b_columns))
      throw std::runtime_error("error: non-square matricies not supported in hmul");
    Multiplier multiplier(*protocol);
    res = multiplier.multiply(A, B);
    expected_res = A * B;
    if (op == OP_HMUL)
      undiff(res);
  } else {
    throw std::runtime_error("unsupported operation");
  }

  if (show_data) {
    print(A, "A");
    print(B, "B");
    print(res, "result");
    print(expected_res, "expected");
  }
  auto abssum = std::accumulate(
      expected_res.begin(), expected_res.end(), 0.0,
      [](double acc, double value) { return acc + std::fabs(value); });
  auto distance = dist(expected_res, res);
  auto eps = distance / abssum;
  std::cout << std::setprecision(3) << std::fixed;
  std::cout << operation_str << ": eps " << eps << std::endl;
  return 0;
} catch (std::exception &e) {
  std::cerr << e.what() << std::endl;
}
