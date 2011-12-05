#include "LinearRegression.h"

bool LinearRegression::FitLinearModel(Matrix & X, Vector & y){
    Matrix Xt;
    Xt.Transpose(X);

    Matrix XtX;
    XtX.Product(Xt, X);
    if (!this->chol.TryDecompose(XtX))
        return false;
    chol.Decompose(XtX);
    chol.Invert();
    this->XtXinv = chol.inv;

    Vector tmp = y;
    tmp.Product(Xt, y);
    this->B.Product(this->XtXinv, tmp);

    this->predict.Product(X, this->B);
    this->residuals = y;
    this->residuals.Subtract(this->predict);

    this->sigma2 = 0.0;
    for (int i = 0; i < this->B.Length(); i++){
        sigma2 += (this->residuals[i]) * (this->residuals[i]);
    }
    sigma2 /= y.Length(); // MLE estimates of sigma2

    this->covB = this->XtXinv;
    this->covB.Multiply(sigma2);
}; 

Vector& LinearRegression::GetAsyPvalue(){
	int numCov = B.Length();
	pValue.Dimension(B.Length());
	for (int i = 0; i < numCov; i ++){
		double Zstat = B[i]/sqrt(covB[i][i]);
		pValue[i] = ndist(Zstat);
		if (pValue[i] >= 0.5){
			pValue[i] = 2*(1-pValue[i]);
		} else pValue[i] = 2*pValue[i];
	}
	return(pValue);
};


//////////////////////////////////////////////////////////////////////
// Score test
LinearRegressionScoreTest::LinearRegressionScoreTest():pvalue(0.0){};
bool LinearRegressionScoreTest::FitLinearModel(Matrix &X, Vector &y, int colToTest) {
    Matrix Xnull;
    Vector Xcol;
    this->splitMatrix(X, colToTest, Xnull, Xcol); // Xnull is the X matrix after taking out xcol
    // LinearRegression lr;
    // if (this->lr.FitLinearModel(Xnull, y, nRound) == false){
    //     return false;
    // }
    if (!this->FitNullModel(Xnull, y)) 
        return false;
    if (!this->TestCovariate(Xnull, y, Xcol))
        return false;
    return true;
}

bool LinearRegressionScoreTest::FitNullModel(Matrix& Xnull, Vector& y){
    if (this->lr.FitLinearModel(Xnull, y) == false){
        return false;
    }
    return true;
};
bool LinearRegressionScoreTest::TestCovariate(Matrix& Xnull, Vector& y, Vector& Xcol){
    double U = 0.0;
    double I = 0.0;

    // printf("size of betaHat = %d\n",betaHat1.Length());

    // define a vector and a matrix for I_r = V_rr - corr*(mat_corr)^{-1}*corr
    //int nParamRemain = X.cols - 1;
    int nParamRemain = Xnull.cols;
    Vector vec_corr;
    vec_corr.Dimension(nParamRemain,0.0);
    Matrix mat_corr;
    mat_corr.Dimension(nParamRemain,nParamRemain,0.0);

    for (int i = 0; i < Xnull.rows; i++){
        U += Xcol[i] * ( y[i] - lr.GetPredicted()[i] );
        // calcualte vec_corr
        for (int j = 0; j < Xnull.cols; j ++) {
            vec_corr[j] += Xcol[i] * Xnull[i][j];
            // printf("j = %d, xcol = %d, Xnull = %d\n",j,bnull, xcol[j], Xnull[i][j]);
        }

        // calcualte mat_corr
        for (int j = 0; j < nParamRemain; j ++) {
            for (int k = 0; k < nParamRemain; k ++) {
                mat_corr[j][k] += Xnull[i][j] * Xnull[i][k];
            }
        }
        I += Xcol[i] * Xcol[i];
    }

    // inverse the mat_corr
    SVD svd;
    svd.InvertInPlace(mat_corr);

    Vector leftMult_corr;
    leftMult_corr.Dimension(nParamRemain,0.0);

    // updates I by substracting the terms led by correlations
    // multiplying vec_corr with mat_corr
    for (int i = 0; i < nParamRemain; i ++) {
        for (int j = 0; j < nParamRemain; j ++) {
            leftMult_corr[i] += vec_corr[j] * mat_corr[i][j];
        }
    }

    // multiplying mat_corr with vec_corr
    for (int i = 0; i < nParamRemain; i ++) {
        I -= leftMult_corr[i] * vec_corr[i];
    }

    I *= this->lr.GetSigma2();

    // printf("In the end, I = %.5f\n",I);
    if (I < 1e-6) {
        this->pvalue = 0.0;
        return false;
    }

    this->pvalue = chidist(U*U/I, 1.0); // use chisq to inverse
    return true;
};

/** NOTE:
 * S_i is column i of the transposed @param Xcol, S_i is m by 1 dimension
 * U = \sum_i (Y_i - \hat{\gamma}^T Z_i ) * S_i
 * V = \hat{\sigma}^2 ( \sum _i S_i^T S_i - (\sum Z_i S_i^T) T  inv(\sum Z_i Z_i^T) (\sum Z_i S_i^T)
 */
bool LinearRegressionScoreTest::TestCovariate(Matrix& Xnull, Vector& y, Matrix& Xcol){
    if (Xnull.rows != y.Length() || y.Length() != Xcol.rows){
        fprintf(stderr, "Incompatible dimensino.\n");
        return false;
    }
    int n = Xcol.rows;
    int m = Xcol.cols;
    int d = Xnull.cols;

    Vector U(m);
    Matrix SS(m,m);
    Matrix SZ(d,m);
    Matrix ZZ(d,d);
    U.Zero();
    SS.Zero();
    SZ.Zero();
    ZZ.Zero();
        
    for (int i = 0; i < n; i ++){
        U += X[i] * this->lr.GetResiduals()[i];

        SS += X[i] * Transpose(X[i]) ;
        SZ += X[i] * Xnull[i];
        
    }

};

bool LinearRegressionScoreTest::TestCovariate(Vector& x, Vector& y){
    // notation is from Danyu Lin's paper
    double sumSi = 0.0;
    double sumSi2 = 0.0; 
    double sumYi = 0.0;
    int l = x.Length();
    for (int i = 0; i < l; i++){
        sumSi += x[i];
        sumSi2 += x[i]*x[i];
        sumYi += y[i];
    };
    double yMean = sumYi / l;
    double U = 0.0;
    double sigma2 = 0.0;
    for (int i = 0; i < l; i++){
        double tmp = y[i] - yMean;
        sigma2 += tmp * tmp;
        U += tmp * x[i];
    };
    sigma2 /= x.Length();

    int n = y.Length();
    double V = sigma2 * (sumSi2 - sumSi / n *sumSi);
    if (V < 1e-6) {
        this->pvalue = 0.0;
        return false;
    }

    this->pvalue = chidist(U*U/V, 1.0); // use chisq to inverse
    return true;
};

bool LinearRegressionScoreTest::TestCovariate(Matrix& x, Vector& y){

};

void LinearRegressionScoreTest::splitMatrix(Matrix& x, int col, Matrix& xnull, Vector& xcol){
    if (x.cols < 2) {
        printf("input matrix has too few cols!\n");
    }
    xnull.Dimension(x.rows, x.cols - 1);
    xcol.Dimension(x.rows);
    for (int i = 0; i < x.rows; i++){
        for (int j = 0; j < x.cols; j++){
            if (j < col){
                xnull[i][j] = x[i][j];
            }else if (j == col){
                xcol[i] = x[i][j];
            } else {
                xnull[i][j-1] = x[i][j];
            }
        }
    }
};
