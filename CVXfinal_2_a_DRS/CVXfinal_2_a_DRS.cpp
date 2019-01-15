#include <iostream>
#include <vector>
#include <mkl.h>
#include "CSVparser.hpp"

// DRS for the primal problem
// min c^T * x
// s.t. A * x = c
//		x >= 0
// x+ = prox_{th}{z}
// h(x) = 1_{x>=0}(x)
// prox_{th}(z) = projection_{R+}(z)
// u+ = prox_{tf}(2x-z)
// f(x) = -c^T * x + 1_{Ax=b}(x)
// prox_{tf}(y) = (y + t*c)-A^T*(A*Y+t*A*c-b)
// z+ = z + u+ + x+


const static int32_t alignment = 32;

std::vector<double_t> gradient_lagrangian(std::vector<double_t> x0, double_t* A, double_t* b, double_t* c, const int32_t m, const int32_t n, int32_t outerCount) {
	std::vector<double_t> result;

	double_t* x;
	double_t* u;
	double_t* w;
	double_t* bminusAx;
	double_t* AAT;
	double_t* ATinvAAT;
	double_t* temp;

	x = (double_t*)mkl_malloc(n * sizeof(double_t), alignment);
	u = (double_t*)mkl_malloc(n * sizeof(double_t), alignment);
	w = (double_t*)mkl_malloc(n * sizeof(double_t), alignment);
	bminusAx = (double_t*)mkl_malloc(m * sizeof(double_t), alignment);
	AAT = (double_t*)mkl_malloc(m * m * sizeof(double_t), alignment);
	ATinvAAT = (double_t*)mkl_malloc(m * n * sizeof(double_t), alignment);
	temp = (double_t*)mkl_malloc(m * sizeof(double_t), alignment);

	int32_t size = m * m;
	int32_t info;
	int32_t *ipiv = new int[m];
	double_t *work = new double[size];

	//AAT = A * A^T
	cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasTrans, m, m, n, 1.0, A, n, A, n, 0.0, AAT, m);
	//AAT = inv(AAT)	
	dgetrf(&m, &m, AAT, &m, ipiv, &info);
	dgetri(&m, AAT, &m, ipiv, work, &size, &info);
	//ATinvAAT = A^T * AAT
	cblas_dgemm(CblasRowMajor, CblasTrans, CblasNoTrans, n, m, m, 1.0, A, n, AAT, m, 0.0, ATinvAAT, m);

	for (int i = 0; i < n; ++i) {
		x[i] = x0[i];
	}
	for (int i = 0; i < n; ++i) {
		u[i] = x0[i + n];
	}
	for (int i = 0; i < n; ++i) {
		w[i] = x0[i + 2*n];
	}

	for (int outer = 0; outer < outerCount; ++outer) {
		//update of u
		//u = x - w - c
		cblas_daxpby(n, 1.0, x, 1, 0.0, u, 1);
		cblas_daxpby(n, -1.0, w, 1, 1.0, u, 1);
		cblas_daxpby(n, -1.0, c, 1, 1.0, u, 1);
		//temp = b - A * x
		cblas_dgemv(CblasRowMajor, CblasNoTrans, m, n, 1.0, A, n, u, 1, 0.0, temp, 1);
		cblas_daxpby(m, 1.0, b, 1, -1.0, temp, 1);
		//u = u + ATinvAAT * temp
		cblas_dgemv(CblasRowMajor, CblasNoTrans, n, m, 1.0, ATinvAAT, m, temp, 1, 1.0, u, 1);

		//update of x
		//x = u + w
		cblas_daxpby(n, 1.0, u, 1, 0.0, x, 1);
		cblas_daxpby(n, 1.0, w, 1, 1.0, x, 1);
		//projection x to R+
		for (int i = 0; i < n; ++i) {
			if (x[i] <= 0) { x[i] = 0.0; }
		}

		//update of w
		//w = w + u - x
		cblas_daxpby(n, 1.0, u, 1, 1.0, w, 1);
		cblas_daxpby(n, -1.0, x, 1, 1.0, w, 1);

		double_t primal = cblas_ddot(n, c, 1, x, 1);
		//double_t dual = -cblas_ddot(m, b, 1, y, 1);
		std::cout << "count: " << outer << "\tprimal: " << primal << std::endl;
		//std::cout << "count: " << outer << "\tprimal: " << primal << "\tdual: " << dual << std::endl;

	}

	for (int i = 0; i < n; ++i) {
		result.push_back(x[i]);
	}
	for (int i = 0; i < n; ++i) {
		result.push_back(u[i]);
	}
	for (int i = 0; i < n; ++i) {
		result.push_back(w[i]);
	}


	mkl_free(x);
	mkl_free(u);
	mkl_free(w);
	mkl_free(bminusAx);
	mkl_free(AAT);
	mkl_free(ATinvAAT);
	mkl_free(temp);
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

	for (int i = 0; i < 3*n; ++i) {
		x.push_back(0.0);
	}

	x = gradient_lagrangian(x, A, b, c, m, n, 1000);

	for (int i = 0; i < n; ++i) {
		std::cout << "x_" << i << "\t" << x[i] << std::endl;
	}

	mkl_free(A);
	mkl_free(b);
	mkl_free(c);
	return 0;
}
