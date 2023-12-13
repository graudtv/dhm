#pragma once

#include <algorithm>
#include <cassert>
#include <iostream>
#include <random>
#include <vector>

namespace dhm {

template <class T> std::vector<T> makeRandomArray(size_t Size) {
  static std::default_random_engine Gen;

  std::vector<T> Arr(Size);
  std::uniform_int_distribution<int> Distrib(-100, 100);

  std::generate(Arr.begin(), Arr.end(), [&]() -> T { return Distrib(Gen); });
  return Arr;
}

template <class T> class Matrix {
  std::vector<T> Data;
  size_t Columns;

public:
  using value_type = T;

  Matrix() : Data(), Columns(0) {}
  Matrix(size_t Rows, size_t Cols) : Data(Rows * Cols), Columns(Cols) {}
  Matrix(std::vector<T> Values, size_t Cols)
      : Data(std::move(Values)), Columns(Cols) {}

  size_t columns() const { return Columns; }
  size_t rows() const { return size() / columns(); }
  size_t size() const { return Data.size(); }
  T *data() { return Data.data(); }
  const T *data() const { return Data.data(); }
  auto begin() { return Data.begin(); }
  auto begin() const { return Data.begin(); }
  auto end() { return Data.end(); }
  auto end() const { return Data.end(); }

  bool empty() const { return !size(); }

  T *beginRow(size_t Row) { return &Data[Row * Columns]; }
  T *endRow(size_t Row) { return beginRow(Row + 1); }
  const T *beginRow(size_t Row) const { return &Data[Row * Columns]; }
  const T *endRow(size_t Row) const { return beginRow(Row + 1); }

  T &operator()(size_t I, size_t J) { return Data[I * Columns + J]; }
  const T &operator()(size_t I, size_t J) const {
    return Data[I * Columns + J];
  }

  Matrix &operator+=(const Matrix &Other) {
    assert(rows() == Other.rows() && columns() == Other.columns() &&
           "incompatible matrices");
    for (size_t I = 0; I < Data.size(); ++I)
      Data[I] += Other.Data[I];
    return *this;
  }

  Matrix getTransposed() const {
    Matrix<T> Result(rows(), columns());
    for (size_t I = 0; I < rows(); ++I)
      for (size_t J = 0; J < columns(); ++J)
        Result(I, J) = (*this)(J, I);
    return Result;
  }

  static Matrix random(size_t Rows, size_t Cols) {
    return Matrix(makeRandomArray<T>(Rows * Cols), Cols);
  }

  friend Matrix operator +(const Matrix &A, const Matrix &B) {
    return Matrix{A} += B;
  }

  friend Matrix operator *(const Matrix &A, const Matrix &B) {
    Matrix Result(A.rows(), B.columns());
    for (size_t I = 0; I < A.rows(); ++I)
      for (size_t J = 0; J < B.columns(); ++J) {
        T Tmp = 0;
        for (size_t K = 0; K < A.columns(); ++K)
          Tmp += A(I, K) * B(K, J);
        Result(I, J) = Tmp;
      }
    return Result;
  }
};

/* Equivalent to A * B.getTransposed() */
template <class T>
Matrix<T> mulT(const Matrix<T> &A, const Matrix<T> &B) {
  Matrix<T> Result(A.rows(), B.columns());
  for (size_t I = 0; I < A.rows(); ++I)
    for (size_t J = 0; J < B.columns(); ++J) {
      T Tmp = 0;
      for (size_t K = 0; K < A.columns(); ++K)
        Tmp += A(I, K) * B(J, K);
      Result(I, J) = Tmp;
    }
  return Result;
}

template <class T>
void print(const Matrix<T> &M, const char *Prefix,
           std::ostream &Os = std::cout) {
  Os << Prefix << " = {\n";
  for (int I = 0; I < M.rows(); ++I) {
    for (int J = 0; J < M.columns(); ++J)
      Os << M(I, J) << " ";
    Os << '\n';
  }
  Os << "}" << std::endl;
}

} // namespace dhm
