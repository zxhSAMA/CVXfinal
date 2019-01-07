#include <iostream>
#include <vector>
#include <mkl.h>
#include "CSVparser.hpp"

//using ADMM to solve the dual problem
//min 0.5 * ||\lambda||_2^2 - b^T\lambda
//s.t.A^T\lambda = s; ||s||infinity <= \mu
//L(\lambda,s,x)=0.5*||\lambda||_2^2-b^T\lambda+x^T(A^T\lambda-s)+0.5*t*||A^T\lambda-s||_2^2
//\lambda^+=(t*A*A^T+I_m)^-1*(t*As+b-Ax)
//s^+=P[-\mu,\mu](A^T\lambda^++x/t)
//x^+=x+t(A^T*\lambda^+-s^+)

const static int32_t alignment = 32;

std::vector<double_t> admm_dual(std::vector<double_t> x0, double_t* A, const int32_t n, const int32_t m, double_t* b, double_t mu, double_t t, int32_t totalCount) {
	std::vector<double_t> result;

	double_t* x;
	double_t* lambda;
	double_t* s;
	double_t* I;
	double_t* temp;

	int32_t size = m * m;
	int32_t info;
	int32_t *ipiv = new int[m];
	double_t *work = new double[size];

	x = (double_t*)mkl_malloc(n * sizeof(double_t), alignment);
	lambda = (double_t*)mkl_malloc(m * sizeof(double_t), alignment);
	s = (double_t*)mkl_malloc(n * sizeof(double_t), alignment);
	I = (double_t*)mkl_malloc(m*m * sizeof(double_t), alignment);
	temp = (double_t*)mkl_malloc(m * sizeof(double_t), alignment);

	for (int i = 0; i < n; ++i) {
		x[i] = x0[i];
		s[i] = 0.0;
	}

	for (int i = 0; i < m; ++i) {
		for (int j = 0; j < m; ++j) {
			if (j == i) { I[i*m + j] = 1.0; }
			else { I[i*m + j] = 0.0; }
		}
	}

	for (int count = 0; count < totalCount; ++count) {
		//update of lambda
		//I = t * A * A^T + I
		cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasTrans, m, m, n, t, A, n, A, n, 1.0, I, m);
		//I = inv(I)	
		dgetrf(&m, &m, I, &m, ipiv, &info);
		dgetri(&m, I, &m, ipiv, work, &size, &info);
		//s = -x + t*s
		cblas_daxpby(n, -1.0, x, 1, t, s, 1);
		//temp = A * s
		cblas_dgemv(CblasRowMajor, CblasNoTrans, m, n, 1.0, A, n, s, 1, 0.0, temp, 1);
		//temp = b + temp
		cblas_daxpby(m, 1.0, b, 1, 1.0, temp, 1);
		//lambda = I * temp;
		cblas_dgemv(CblasRowMajor, CblasNoTrans, m, m, 1.0, I, m, temp, 1, 0.0, lambda, 1);
		//update of s
		//s = A^T * lambda
		cblas_dgemv(CblasRowMajor, CblasTrans, m, n, 1.0, A, n, lambda, 1, 0.0, s, 1);
		//s = s + 1/t * x
		cblas_daxpby(n, 1 / t, x, 1, 1.0, s, 1);

		for (int i = 0; i < n; ++i) {
			if (s[i] > mu) { s[i] = mu; }
			else if (s[i] < -mu) { s[i] = -mu; }
		}
		//update of x
		//x = t * A^T * lambda + x
		cblas_dgemv(CblasRowMajor, CblasTrans, m, n, t, A, n, lambda, 1, 1.0, x, 1);
		//x = -t * s + x
		cblas_daxpby(n, -t, s, 1, 1.0, x, 1);

		//temp = A * x
		cblas_dgemv(CblasRowMajor, CblasNoTrans, m, n, 1.0, A, n, x, 1, 0.0, temp, 1);
		//temp = -b + temp
		cblas_daxpby(m, -1.0, b, 1, 1.0, temp, 1);
		double_t f_x = 0.5*cblas_dnrm2(m, temp, 1)*cblas_dnrm2(m, temp, 1) + mu * cblas_dasum(n, x, 1);
		std::cout << "count : " << count << "\tf_x : " << f_x << std::endl;

	}

	for (int i = 0; i < n; ++i) {
		result.push_back(x[i]);
	}

	delete[] ipiv;
	delete[] work;
	mkl_free(x);
	mkl_free(lambda);
	mkl_free(s);
	mkl_free(I);
	mkl_free(temp);
	return result;
}

int main(int argc, char** argv)
{
	double_t* A;
	double_t* b;
	std::vector<double_t> x0;

	double_t mu = 1.0e-3;

	std::vector<double_t> x;

	const int32_t n = 1024;
	const int32_t m = 512;

	A = (double_t*)mkl_malloc(m * n * sizeof(double_t), alignment);
	b = (double_t*)mkl_malloc(m * sizeof(double_t), alignment);

	for (int i = 0; i < n; ++i) {
		x0.push_back(0.0);
	}

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

	//x = admm_dual(x0, A, n, m, b, mu * 1000, 0.01,1000);
	//x = admm_dual(x, A, n, m, b, mu * 100, 0.01,1000);
	//x = admm_dual(x, A, n, m, b, mu * 10, 0.01,1000);
	x = admm_dual(x0, A, n, m, b, mu * 1, 10, 1000);

	for (int i = 0; i < n; ++i) {
		std::cout << "x_" << i << "\t" << x[i] << std::endl;
	}

	mkl_free(A);
	mkl_free(b);
	return 0;
}
