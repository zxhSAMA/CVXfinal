#include <iostream>
#include <vector>
#include <mkl.h>
#include "CSVparser.hpp"

// Apply a gradient-type method to minimize augmented Lagrangian function
// min -b^y
// s.t. A^T * y + s = c
//		s >= 0
// L = -b^T * y + 1/(2*sigma)(||P_+(x-\sigma(c-A^T*y))||_2^2-||x||_s^2) 
// \nabla{L} = -b+AP_+(x-\sigma(c-A^T*y))
// y_+ = y - t*\nabla{L}
// x_+ = P_+(x-\sigma(c-A^T*y_+))

const static int32_t alignment = 32;

std::vector<double_t> gradient_lagrangian(std::vector<double_t> x0, double_t* A, double_t* b, double_t* c, const int32_t m, const int32_t n, double_t t, double_t sigma, int32_t innerCount, int32_t outerCount) {
	std::vector<double_t> result;

	double_t* x;
	double_t* y;
	double_t* projection;
	double_t* gradient;

	x = (double_t*)mkl_malloc(n * sizeof(double_t), alignment);
	y = (double_t*)mkl_malloc(m * sizeof(double_t), alignment);
	projection = (double_t*)mkl_malloc(n * sizeof(double_t), alignment);
	gradient = (double_t*)mkl_malloc(m * sizeof(double_t), alignment);

	for (int i = 0; i < n; ++i) {
		x[i] = x0[i];
	}
	for (int i = 0; i < m; ++i) {
		y[i] = x0[i+n];
	}

	for (int outer = 0; outer < outerCount; ++outer) {
		//update of y
		for (int inner = 0; inner < innerCount; ++inner){
			//projection = A^T * y
			cblas_dgemv(CblasRowMajor, CblasTrans, m, n, 1.0, A, n, y, 1, 0.0, projection, 1);
			//projection = c - projection
			cblas_daxpby(n, 1.0, c, 1, -1.0, projection, 1);
			//projection = x -sigma * projection
			cblas_daxpby(n, 1.0, x, 1, -sigma, projection, 1);
			//project to R+
			for (int i = 0; i < n; ++i){
				if (projection[i] < 0) { projection[i] = 0; }
			}
			//gradient = A * projection
			cblas_dgemv(CblasRowMajor, CblasNoTrans, m, n, 1.0, A, n, projection, 1, 0.0, gradient, 1);
			//gradient = -b + gradient
			cblas_daxpby(m, -1.0, b, 1, 1.0, gradient, 1);
			//y = -t * gradient + y;
			cblas_daxpby(m, -t, gradient, 1, 1.0, y, 1);

		}
		//x = projection
		cblas_daxpby(n, 1.0, projection, 1, 0.0, x, 1);

		double_t primal = cblas_ddot(n, c, 1, x, 1);
		double_t dual = -cblas_ddot(m, b, 1, y, 1);
		//std::cout << "outer count: " << outer << "\tinner count:  " << inner << "\tprimal: " << primal << "\tdual: " << dual << std::endl;
		std::cout << "outer count: " << outer << "\tprimal: " << primal << "\tdual: " << dual << std::endl;

		
	}

	for (int i = 0; i < n; ++i) {
		result.push_back(x[i]);
	}
	for (int i = 0; i < m; ++i) {
		result.push_back(y[i]);
	}

	mkl_free(x);
	mkl_free(y);
	mkl_free(projection);
	mkl_free(gradient);

	return result;
}

int main(int argc, char** argv){
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

	for (int i = 0; i < m + n; ++i) {
		x.push_back(0.0);
	}

	x = gradient_lagrangian(x, A, b, c, m, n, 0.001, 0.01, 1000,2000);

	for (int i = 0; i < n; ++i) {
		std::cout << "x_" << i << "\t" << x[i] << std::endl;
	}

	mkl_free(A);
	mkl_free(b);
	mkl_free(c);
	return 0;
}
