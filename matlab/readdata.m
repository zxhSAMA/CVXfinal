A = csvread('A.csv');
b = csvread('b.csv');
c = csvread('c.csv');

m = 20;
n = 100;

cvx_solver gurobi
cvx_begin
    variable x(n);
    minimize( c' * x );
    subject to
        A * x == b
        x >= 0
cvx_end

x

cvx_solver gurobi
cvx_begin
    variable y(m);
    minimize( -b' * y );
    subject to
        A' * y - c <= 0
cvx_end

y