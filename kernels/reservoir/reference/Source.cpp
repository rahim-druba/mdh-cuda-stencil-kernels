#include<iostream>
#include<stdio.h>
#include<fstream>
#include<ctime>
using namespace std;
double inversionMatrix(double *A, int N)
{
	double temp;
	double *B;
	B = (double*)malloc(N * N * sizeof(double));
	for (int i = 0; i < N; i++)
		for (int j = 0; j < N; j++)
		{
			B[i*N + j] = 0.0;

			if (i == j)
				B[i*N + j] = 1.0;
		}

	for (int k = 0; k < N; k++)
	{
		temp = A[k*N + k];

		for (int j = 0; j < N; j++)
		{
			A[k*N + j] /= temp;
			B[k*N + j] /= temp;
		}

		for (int i = k + 1; i < N; i++)
		{
			temp = A[i*N + k];

			for (int j = 0; j < N; j++)
			{
				A[i*N + j] -= A[k*N + j] * temp;
				B[i*N + j] -= B[k*N + j] * temp;
			}
		}
	}

	for (int k = N - 1; k > 0; k--)
	{
		for (int i = k - 1; i >= 0; i--)
		{
			temp = A[i*N + k];

			for (int j = 0; j < N; j++)
			{
				A[i*N + j] -= A[k*N + j] * temp;
				B[i*N + j] -= B[k*N + j] * temp;
			}
		}
	}

	for (int i = 0; i < N; i++)
		for (int j = 0; j < N; j++)
			A[i*N + j] = B[i*N + j];

	delete(B);
	return 0;
}
/*string convertInt(int number)
{
if (number == 0)
return "0";
string temp = "";
string returnvalue = "";
while (number>0) {
temp += number % 10 + 48;
number /= 10;
}
for (int i = 0; i<temp.length(); i++)
returnvalue += temp[temp.length() - i - 1];
return returnvalue;
}*/
int main() {
	typedef struct CrsMatrix
	{
		int N; // Размер матрицы (N x N)
		int NZ; // Кол-во ненулевых элементов
		double* Value; // Массив значений (размер NZ)
		int* Col; // Массив номеров столбцов (размер NZ)
		int* RowIndex; // Массив индексов строк (размер N +1)
	}
	crsMatrix;

	/*FILE * outputfile;
	string fname1 = "oil";
	string pre = "000";
	string suf = ".dat";
	string outname;
	outname = fname1 + convertInt(1) + suf;
	outputfile = fopen(outname.c_str(), "w");
	ofstream fout("oildecomposition. txt");*/

	int size, n, m, restr, itmax, flag, i, j, k, l, itr, fulln, iteration, NZ, ii;
	int CLOCKS_PER_MSEC = CLOCKS_PER_SEC;
	fulln = 50;
	n = (fulln - 2)*(fulln - 2) - 2;
	NZ = n + 4 * (n - fulln + 2);
	itmax = 2000;
	restr = 300;
	m = restr;
	double eps = 0.0001;
	double betta, norma;
	double t, temp, bnrm, error, h, xh, yh;
	h = pow(fulln - 1, -1);
	double *S;
	S = (double*)malloc(fulln*fulln * sizeof(double));
	double *P;
	P = (double*)malloc(fulln*fulln * sizeof(double));
	double *Mx;
	Mx = (double*)malloc(fulln*fulln * sizeof(double));
	double *My;
	My = (double*)malloc(fulln*fulln * sizeof(double));
	double *V;
	V = (double*)malloc(n * (m + 1) * sizeof(double));
	double *H;
	H = (double*)malloc((m + 1) * m * sizeof(double));
	double *R;
	R = (double*)malloc(m*m * sizeof(double));
	double kx = 0.001, ky = 0.001;
	double m1 = 0.03, m2 = 0.3;
	double dt = (h*h) / 0.00001;
	double *x0, *b, *r, *w, *e, *q, *c, *s, *y;
	b = (double*)malloc(n * sizeof(double));
	x0 = (double*)malloc(n * sizeof(double));
	r = (double*)malloc(n * sizeof(double));
	w = (double*)malloc(n * sizeof(double));
	e = (double*)malloc(m * sizeof(double));
	q = (double*)malloc((m + 1) * sizeof(double));
	c = (double*)malloc(m * sizeof(double));
	s = (double*)malloc(m * sizeof(double));
	y = (double*)malloc(m * sizeof(double));

	crsMatrix *A;
	A = (crsMatrix*)malloc(sizeof(crsMatrix));
	A->N = n;
	A->NZ = NZ;
	A->Value = (double*)malloc(A->NZ * sizeof(double));
	A->Col = (int*)malloc(A->NZ * sizeof(int));
	A->RowIndex = (int*)malloc((A->N + 1) * sizeof(int));



	for (i = 0; i < fulln; i++) {
		for (j = 0; j < fulln; j++) {
			P[i*fulln + j] = 0.3;
			S[i*fulln + j] = 0.1;
		}
	}
	P[1 * fulln + 1] = 0.1;
	P[(fulln - 2)*fulln + fulln - 2] = 0.5;

	//for (iteration = 0; iteration < 1; iteration++) {
	m = restr;
	for (i = 0; i < fulln; i++) {
		for (j = 0; j < fulln; j++) {
			Mx[i*fulln + j] = -(kx*((S[i*fulln + j] * S[i*fulln + j]) / m1) + kx*(((1 - S[i*fulln + j]) * (1 - S[i*fulln + j])) / m2));
			My[i*fulln + j] = -(ky*((S[i*fulln + j] * S[i*fulln + j]) / m1) + ky*(((1 - S[i*fulln + j]) * (1 - S[i*fulln + j])) / m2));
		}
	}
	for (i = 0; i < n; i++) {
		for (j = 0; j < m + 1; j++) {
			V[i*(m + 1) + j] = 0;
		}
	}
	for (i = 0; i < m + 1; i++) {
		for (j = 0; j < m; j++) {
			H[i*m + j] = 0;
		}
	}
	for (i = 0; i < m; i++) {
		c[i] = 0;
		s[i] = 0;
	}
	l = 1;
	k = 2;
	ii = 0;
	for (i = 0; i < n; i++) {
		A->RowIndex[i] = ii;
		for (j = 0; j < n; j++) {
			if (i == j + (fulln - 2)) {
				A->Value[ii] = 0.5*(Mx[l*fulln + k] + Mx[(l - 1)*fulln + k]);//a 
				if (l == fulln - 2) {
					A->Value[ii] += 0.5*(Mx[(l + 1)*fulln + k] + Mx[l*fulln + k]);//+e
				}
				A->Col[ii] = j;
				ii++;
			}
			else if (i == j + 1) {
				if (k != 1) {
					A->Value[ii] = 0.5*(My[l*fulln + k] + My[l*fulln + k - 1]);//b
					if (k == fulln - 2) {
						A->Value[ii] += 0.5*(My[l*fulln + k + 1] + My[l*fulln + k]);//+d
					}
					A->Col[ii] = j;
					ii++;
				}
			}
			else if (i == j) {
				A->Value[ii] = -(0.5*(My[l*fulln + k] + My[l*fulln + k - 1]) + 0.5*(My[l*fulln + k + 1] + My[l*fulln + k]) + 0.5*(Mx[(l + 1)*fulln + k] + Mx[l*fulln + k]) + 0.5*(Mx[l*fulln + k] + Mx[(l - 1)*fulln + k]) + (h*h) / dt);//c
				A->Col[ii] = j;
				ii++;
			}
			else if (i == j - 1) {
				if (k != fulln - 2) {
					A->Value[ii] = 0.5*(My[l*fulln + k + 1] + My[l*fulln + k]);//d
					if (k == 1) {
						A->Value[ii] += 0.5*(My[l*fulln + k] + My[l*fulln + k - 1]);//+b
					}
					A->Col[ii] = j;
					ii++;
				}
			}
			else if (i == j - (fulln - 2)) {
				A->Value[ii] = 0.5*(Mx[(l + 1)*fulln + k] + Mx[l*fulln + k]);//e
				if (l == 1) {
					A->Value[ii] += 0.5*(Mx[l*fulln + k] + Mx[(l - 1)*fulln + k]);//+a
				}
				A->Col[ii] = j;
				ii++;
			}
		}
		k++;
		if (k == fulln - 1) {
			l++;
			k = 1;
		}
	}
	A->RowIndex[n] = NZ;
	k = 0;
	for (i = 1; i < fulln - 1; i++) {
		for (j = 1; j < fulln - 1; j++) {
			if (((i == 1) && (j == 1)) || ((i == fulln - 2) && (j == fulln - 2))) continue;
			b[k] = -(((h*h) / dt)*P[i*fulln + j]);
			if ((i == 1) && (j == 2)) {
				b[k] -= P[1 * fulln + 1] * 0.5*(My[i*fulln + j] + My[i*fulln + j - 1]);
			}
			if ((i == 2) && (j == 1)) {
				b[k] -= P[1 * fulln + 1] * 0.5*(Mx[i*fulln + j] + Mx[(i - 1)*fulln + j]);
			}
			if ((i == fulln - 3) && (j == fulln - 2)) {
				b[k] -= P[(fulln - 2) * fulln + (fulln - 2)] * 0.5*(Mx[(i + 1)*fulln + j] + Mx[i*fulln + j]);
			}
			if ((i == fulln - 2) && (j == fulln - 3)) {
				b[k] -= P[(fulln - 2) * fulln + (fulln - 2)] * 0.5*(My[i*fulln + j + 1] + My[i*fulln + j]);
			}
			k++;
		}
	}

	//cout << endl;
	int t3 = clock();
	for (i = 0; i < n; i++) {
		x0[i] = 1;
		//cout << b[i] << endl;
	}
	e[0] = 1;
	for (i = 1; i < m; i++) {
		e[i] = 0;
	}
	betta = bnrm = 0;
	for (i = 0; i < n; i++) {
		bnrm += b[i] * b[i];
	}
	bnrm = sqrt(bnrm);
	if (bnrm == 0) bnrm = 1;
	for (int itr = 1; itr <= itmax; itr++) {
		m = restr;
		for (i = 0; i < n; i++) {
			temp = 0;
			for (j = A->RowIndex[i]; j < A->RowIndex[i + 1]; j++) {
				temp += A->Value[j] * x0[A->Col[j]];
			}
			r[i] = b[i] - temp;
		}
		betta = 0;
		for (i = 0; i < n; i++) {
			betta += r[i] * r[i];
		}
		betta = sqrt(betta);
		error = betta / bnrm;
		if (error < eps) {
			return 0;
		}
		for (i = 0; i < n; i++) {
			V[i*(m + 1)] = r[i] / betta;
		}
		for (i = 0; i < m; i++) {
			q[i] = e[i] * betta;
		}
		for (j = 0; j < m; j++) {
			ii = j;
			for (i = 0; i < n; i++) {
				w[i] = 0;
				for (k = A->RowIndex[i]; k < A->RowIndex[i + 1]; k++) {
					w[i] += A->Value[k] * V[j + A->Col[k] * (m + 1)];
				}
			}
			for (i = 0; i <= j; i++) {
				for (k = 0; k < n; k++) {
					H[i*m + j] += V[i + k*(m + 1)] * w[k];
				}
				for (k = 0; k < n; k++) {
					w[k] = w[k] - H[i*m + j] * V[i + k*(m + 1)];
				}
			}
			norma = 0;
			for (k = 0; k < n; k++) {
				norma += w[k] * w[k];
			}
			norma = sqrt(norma);
			H[(j + 1)*m + j] = norma;
			for (k = 0; k < n; k++) {
				V[(j + 1) + k*(m + 1)] = w[k] / H[(j + 1) * m + j];
			}
			for (i = 0; i < j; i++) {
				temp = c[i] * H[i*m + j] + s[i] * H[(i + 1)*m + j];
				H[(i + 1)*m + j] = -s[i] * H[i*m + j] + c[i] * H[(i + 1)*m + j];
				H[i*m + j] = temp;
			}
			if (H[(j + 1)*m + j] == 0) {
				c[j] = 1;
				s[j] = 0;
			}
			else if (fabs(H[j*m + j]) > fabs(H[(j + 1)*m + j])) {
				t = H[(j + 1)*m + j] / H[j*m + j];
				c[j] = 1 / sqrt(1 + t*t);
				s[j] = c[j] * t;
			}
			else {
				t = H[j*m + j] / H[(j + 1)*m + j];
				s[j] = 1 / sqrt(1 + t*t);
				c[j] = s[j] * t;
			}
			temp = c[j] * q[j];
			q[j + 1] = -s[j] * q[j];
			q[j] = temp;
			H[j*m + j] = c[j] * H[j*m + j] + s[j] * H[(j + 1)*m + j];
			H[(j + 1)*m + j] = 0;
			error = fabs(q[j + 1]) / betta;
			if (error <= eps) {
				m = j + 1;
				break;
			}
		}
		cout << endl << "iter=" << itr << "  " << m << endl;
		for (i = 0; i < m; i++) {
			for (j = 0; j < m; j++) {
				R[i*m + j] = H[i*restr + j];
			}
		}
		inversionMatrix(R, m);
		for (i = 0; i < m; i++) {
			y[i] = 0;
			for (j = 0; j < m; j++) {
				y[i] += R[i*m + j] * q[j];
			}
		}

		for (i = 0; i < n; i++) {
			temp = 0;
			for (j = 0; j < m; j++) {
				temp += V[i*(restr + 1) + j] * y[j];
			}
			x0[i] += temp;
		}
		if (error <= eps) {
			break;
		}

		/*for (i = 0; i < n; i++) {
		temp = 0;
		for (j = A->RowIndex[i]; j < A->RowIndex[i + 1]; j++) {
		temp += A->Value[j] * x0[A->Col[j]];
		}
		r[i] = b[i] - temp;
		}
		betta = 0;
		for (i = 0; i < n; i++) {
		betta += r[i] * r[i];
		}
		betta = sqrt(betta);

		error = betta / bnrm;
		if (error <= eps) {
		break;
		}*/

	}
	/*{
	cout << "need more itmax";
	}*/
	int t4 = clock();
	l = 1;
	k = 2;
	for (i = 0; i < n; i++) {
		P[l*fulln + k] = x0[i];
		k++;
		if (k == fulln - 1) {
			l++;
			k = 1;
		}
	}
	for (i = 1; i < fulln - 1; i++) {
		P[i*fulln + 0] = P[i*fulln + 2];
		P[i*fulln + (fulln - 1)] = P[i*fulln + (fulln - 3)];
	}
	for (i = 0; i < fulln; i++) {
		P[0 * fulln + i] = P[2 * fulln + i];
		P[(fulln - 1)*fulln + i] = P[(fulln - 3)*fulln + i];
	}
	for (i = 0; i < fulln; i++) {
		cout << endl;
		for (j = 0; j < fulln; j++) {
			cout << P[i*fulln + j] << " ";
		}
	}
	/*fprintf(outputfile, "TITLE=\"USERData\"\nVARIABLES=i, j, P\n");
	fprintf(outputfile, "ZONE T=\"ZONE1\", i=%d j=%d f=Point\n", fulln, fulln);
	for (int i = 0; i<fulln; i++) {
	for (int j = 0; j<fulln; j++) {
	double x = i * h;
	double y = j * h;
	fprintf(outputfile, "%.8f\t%.8f\t%.8f\t\n",
	i*h, j*h, P[i*fulln + j]);
	}
	}
	fclose(outputfile);*/


	free(P);
	free(S);
	free(Mx);
	free(My);
	free(H);
	free(V);
	free(R);
	free(b);
	free(x0);
	free(r);
	free(w);
	free(e);
	free(q);
	free(c);
	free(s);
	free(y);
	free(A->Value);
	free(A->Col);
	free(A->RowIndex);

	//	int CLOCKS_PER_MSEC = CLOCKS_PER_SEC;
	int time_gmres = t4 - t3;
	cout << endl << "gmres time=" << ((double)time_gmres) / CLOCKS_PER_MSEC << endl;
	system("pause");
	return 0;
}