#include <iostream>
#include <vector>
#include <mkl.h>
#include "CSVparser.hpp"

// ADMM for the dual problem
// min -b^y
// s.t. A^T * y + s = c
//		s >= 0
// L = -b^T * y + x^T * (A^T*y+s-c)+t/2||A^T*y+s-c||_2^2
// ADMM:
// y+ = argmin_y{L}
// (tAA^T+kI)y=b-A(x-t(c-s))
// s+ = argmin_{ s >= 0 }{L}
// s = -x/t +c -A^T*y
// x+ = x+t(A^T*y + s -c)


const static int32_t alignment = 32;

std::vector<double_t> gradient_lagrangian(std::vector<double_t> x0, double_t* A, double_t* b, double_t* c, const int32_t m, const int32_t n, double_t k, double_t t, int32_t outerCount) {
	std::vector<double_t> result;

	double_t* x;
	double_t* y;
	double_t* s;
	double_t* I;
	double_t* tempm;
	double_t* tempn;


	x = (double_t*)mkl_malloc(n * sizeof(double_t), alignment);
	y = (double_t*)mkl_malloc(m * sizeof(double_t), alignment);
	s = (double_t*)mkl_malloc(n * sizeof(double_t), alignment);
	I = (double_t*)mkl_malloc(m * m * sizeof(double_t), alignment);
	tempm = (double_t*)mkl_malloc(m * sizeof(double_t), alignment);
	tempn = (double_t*)mkl_malloc(n * sizeof(double_t), alignment);

	int32_t size = m * m;
	int32_t info;
	int32_t *ipiv = new int[m];
	double_t *work = new double[size];

	for (int i = 0; i < n; ++i) {
		x[i] = x0[i];
	}
	for (int i = 0; i < n; ++i) {
		s[i] = x0[i+ n];
	}
	for (int i = 0; i < m; ++i) {
		y[i] = x0[i + 2*n];
	}

	for (int i = 0; i < m; ++i) {
		for (int j = 0; j < m; ++j) {
			if (j == i) { I[i*m + j] = 1.0; }
			else { I[i*m + j] = 0.0; }
		}
	}

	for (int outer = 0; outer < outerCount; ++outer) {
		//update of y
		//I = t * A * A^T + k * I
		cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasTrans, m, m, n, t, A, n, A, n, k, I, m);
		//I = inv(I)	
		dgetrf(&m, &m, I, &m, ipiv, &info);
		dgetri(&m, I, &m, ipiv, work, &size, &info);
		//tempn = x - t * c + t * s
		cblas_daxpby(n, 1.0, x, 1, 0.0, tempn, 1);
		cblas_daxpby(n, -t, c, 1, 1.0, tempn, 1);
		cblas_daxpby(n, t, s, 1, 1.0, tempn, 1);
		//tempm = A * tempn
		cblas_dgemv(CblasRowMajor, CblasNoTrans, m, n, 1.0, A, n, tempn, 1, 0.0, tempm, 1);
		//tempm = b - tempm
		cblas_daxpby(m, 1.0, b, 1, -1.0, tempm, 1);
		//y = I * tempm
		cblas_dgemv(CblasRowMajor, CblasNoTrans, m, m, 1.0, I, m, tempm, 1, 0.0, y, 1);

		//update of s
		//tempn = A^T * y
		cblas_dgemv(CblasRowMajor, CblasTrans, m, n, 1.0, A, n, y, 1, 0.0, tempn, 1);
		//s = -1/t * x + c - tempn
		cblas_daxpby(n, -1.0/t, x, 1, 0.0, s, 1);
		cblas_daxpby(n, 1.0, c, 1, 1.0, s, 1);
		cblas_daxpby(n, -1.0, tempn, 1, 1.0, s, 1);
		for (int i = 0; i < n; ++i) {
			if (s[i] < 0) { s[i] = 0; }
		}


		//update of x
		//x = x + t * tempn + t * s -t * c
		cblas_daxpby(n, t, tempn, 1, 1.0, x, 1);
		cblas_daxpby(n, t, s, 1, 1.0, x, 1);
		cblas_daxpby(n, -t, c, 1, 1.0, x, 1);


		double_t primal = cblas_ddot(n, c, 1, x, 1);
		double_t dual = -cblas_ddot(m, b, 1, y, 1);

		std::cout << "count: " << outer << "\tprimal: " << primal << "\tdual: " << dual << std::endl;

	}

	for (int i = 0; i < n; ++i) {
		result.push_back(x[i]);
	}
	for (int i = 0; i < n; ++i) {
		result.push_back(s[i]);
	}
	for (int i = 0; i < m; ++i) {
		result.push_back(y[i]);
	}

	mkl_free(x);
	mkl_free(y);
	mkl_free(s);
	mkl_free(I);
	mkl_free(tempm);
	mkl_free(tempn);
	delete[] ipiv;
	delete[] work;

	return result;
}

int main(int argc, char** argv) {
	const int32_t n = 100;
	const int32_t m = 20;

	double_t* A;
	double_t* b;
	double_t* c;

	A = (double_t*)mkl_malloc(m * n * sizeof(double_t), alignment);
	b = (double_t*)mkl_malloc(m * sizeof(double_t), alignment);
	c = (double_t*)mkl_malloc(n * sizeof(double_t), alignment);

	csv::Parser A_csv = csv::Parser("A.csv");
	for (int i = 0; i < n; ++i) {
		A[i] = atof(A_csv.getHeaderElement(i).c_str());
	}
	for (int i = 1; i < m; ++i) {
		for (int j = 0; j < n; ++j) {
			A[i*n + j] = atof(A_csv[i - 1][j].c_str());
		}
	}

	csv::Parser b_csv = csv::Parser("b.csv");
	b[0] = atof(b_csv.getHeaderElement(0).c_str());
	for (int i = 1; i < m; i++) {
		b[i] = atof(b_csv[i - 1][0].c_str());
	}

	csv::Parser c_csv = csv::Parser("c.csv");
	c[0] = atof(c_csv.getHeaderElement(0).c_str());
	for (int i = 1; i < n; i++) {
		c[i] = atof(c_csv[i - 1][0].c_str());
	}

	std::vector<double_t> x;

	for (int i = 0; i < m + n + n; ++i) {
		x.push_back(0.0);
	}

	x = gradient_lagrangian(x, A, b, c, m, n, 0.00001, 10, 3000);

	for (int i = 0; i < n; ++i) {
		std::cout << "x_" << i << "\t" << x[i] << std::endl;
	}

	mkl_free(A);
	mkl_free(b);
	mkl_free(c);
	return 0;
}
