#pragma once

#include "matrix.h"
#include "protocol.h"
#include "splitter.h"

namespace dhm {

/* Base class for all operations working via CommunicationProtocol */
template <class DataT> class OperationBase {
protected:
  CommunicationProtocol<DataT> &protocol;

  OperationBase(CommunicationProtocol<DataT> &p) : protocol(p) {}

  void startAll(Operation op) {
    auto worker_count = protocol.getWorkerCount();
    for (size_t i = 0; i < worker_count; ++i)
      protocol.start(i, op);
  }

  Matrix<DataT> waitAll(const WorkSplitterLinear &splitter, unsigned rows,
                        unsigned columns) {
    Matrix<DataT> result(rows, columns);
    auto worker_count = protocol.getWorkerCount();
    for (size_t i = 0; i < worker_count; ++i) {
      auto work_range = splitter.getRange(i);
      auto chunk = protocol.waitResult(i);
      assert(chunk.rows() == work_range.size());
      assert(chunk.columns() == columns);
      std::copy(chunk.begin(), chunk.end(),
                result.beginRow(work_range.FirstIdx));
    }
    return result;
  }
};

/* Echo operation. Each worker receives part of matrix and sens it back */
template <class DataT> class Echo : public OperationBase<DataT> {
public:
  Echo(CommunicationProtocol<DataT> &p) : OperationBase<DataT>(p) {}

  Matrix<DataT> echo(const Matrix<DataT> &A) {
    auto worker_count = this->protocol.getWorkerCount();
    assert(worker_count > 0 && "no workers");

    this->startAll(OP_ECHO);
    WorkSplitterLinear splitter(A.rows(), worker_count);
    for (size_t i = 0; i < worker_count; ++i) {
      auto work_range = splitter.getRange(i);
      this->protocol.offload(i, A.beginRow(work_range.FirstIdx),
                             work_range.size(), A.columns());
    }
    return this->waitAll(splitter, A.rows(), A.columns());
  }
};

/* Performs addition of two matrices */
template <class DataT> class Adder : public OperationBase<DataT> {
public:
  Adder(CommunicationProtocol<DataT> &p) : OperationBase<DataT>(p) {}

  Matrix<DataT> add(const Matrix<DataT> &A, const Matrix<DataT> &B) {
    assert(A.rows() == B.rows());
    assert(A.columns() == B.columns());
    auto worker_count = this->protocol.getWorkerCount();
    assert(worker_count > 0 && "no workers");

    WorkSplitterLinear splitter(A.rows(), worker_count);
    this->startAll(OP_ADD);
    for (size_t i = 0; i < worker_count; ++i) {
      auto work_range = splitter.getRange(i);
      this->protocol.offload(i, A.beginRow(work_range.FirstIdx), work_range.size(),
                       A.columns());
      this->protocol.offload(i, B.beginRow(work_range.FirstIdx), work_range.size(),
                       B.columns());
    }
    return this->waitAll(splitter, A.rows(), A.columns());
  }
};

/* Performs multiplication of two matrices */
template <class DataT> class Multiplier : public OperationBase<DataT> {
public:
  Multiplier(CommunicationProtocol<DataT> &p) : OperationBase<DataT>(p) {}

  Matrix<DataT> multiply(const Matrix<DataT> &A, const Matrix<DataT> &B) {
    assert(A.columns() == B.rows());
    auto worker_count = this->protocol.getWorkerCount();
    assert(worker_count > 0 && "no workers");

    auto BT = B.getTransposed();
    WorkSplitterLinear splitter(A.rows(), worker_count);
    this->startAll(OP_MUL);
    for (size_t i = 0; i < worker_count; ++i) {
      auto work_range = splitter.getRange(i);
      this->protocol.offload(i, A.beginRow(work_range.FirstIdx), work_range.size(),
                       A.columns());
      this->protocol.offloadMatrix(i, BT);
    }
    return this->waitAll(splitter, A.rows(), A.columns());
  }
};

} // namespace dhm
