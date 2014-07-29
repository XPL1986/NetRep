#define ARMA_USE_LAPACK
#define ARMA_NO_DEBUG
#define ARMA_DONT_USE_CXX11

#include <RcppArmadillo.h>
using namespace Rcpp;
using namespace arma;

// [[Rcpp::depends(BH, bigmemory, RcppArmadillo)]]
#include <bigmemory/BigMatrix.h>

//' Network subset eigenvector and proportion of variance explained in C++
//' 
//' @param pDat SEXP container for the pointer to the data matrix used in 
//'   network construction.
//' @param pScaledDat SEXP container for the pointer to a scaled version of the 
//'   data matrix used to construct the network.
//' @param subsetIndices indices of the network subset of interest in 
//'   \code{pDat}.
//' 
//' @return
//'  A list whose first element is the subset membership for each node (see 
//'  details), and whose second element is the proportion of the variance
//'  explained by the subset's summary vector (see details).
//'
//' @references
//'   \enumerate{
//'     \item{
//'       Langfelder, P., Luo, R., Oldham, M. C. & Horvath, S. \emph{Is my 
//'       network module preserved and reproducible?} PLoS Comput. Biol. 
//'       \strong{7}, e1001057 (2011). 
//'     }
//'  }
//'  
//' @details
//'  First, a summary vector is calculated for the network subset from the 
//'  underlying data. This is the first right singular vector from a singular 
//'  value decomposition (also the eigenvector of the first principal component 
//'  \emph{(1)}). The sign of the returned eigenvector is modified to match the
//'  average of \code{pDat}. This is to match the behaviour of
//'  \emph{moduleEigengenes} in the \code{WGCNA} package.
//'  
//'  Using this summary vector, the subset membership of each node is quantified
//'  as the correlation between that node's data, and the summary vector.
//'  
//'  The proportion of variance explained by this summary vector is quantified
//'  as the average square of the subset memberships for all nodes in the 
//'  network subset.
//'  
//'  The two returned properties are bundled together into one function because
//'  the calculation of the proportion of variance requires much of the same
//'  underlying intermediate calculations that obtaining the first eigenvector
//'  requires.
//' 
//' @import RcppArmadillo
//' @rdname dataSummary-cpp
//'  
// [[Rcpp::export]]
List DataProps(
  SEXP pDat, SEXP pScaledDat, IntegerVector subsetIndices
) {
  XPtr<BigMatrix> xpDat(pDat);
  XPtr<BigMatrix> xspDat(pScaledDat);
  
  // Make sure we're not indexing out of range.
  if (is_true(any(subsetIndices <= 0)) || 
      is_true(any(subsetIndices > xpDat->nrow()))) {
    throw std::out_of_range("Some of requested indices are outside of range!");
  }
  // Make sure pScaledDat corresponds to pDat
    if (xpDat->ncol() != xspDat->ncol() ||
      xpDat->nrow() != xspDat->nrow()) {
    throw Rcpp::exception(
        "The results matrix must have the same dimensions as the data matrix!"
      );
  }
  if (xpDat->matrix_type() != xspDat->matrix_type()) {
    throw Rcpp::exception(
        "The results matrix must have the same 'type' as the data matrix."
      );
  }
  
  // Dispatch function for all types of big.matrix.
  if (xpDat->matrix_type() == 8) {
    // Cast the BigMatrix to an arma::Mat<double>
    mat aDat((double *)xpDat->matrix(), xpDat->nrow(), xpDat->ncol(), false);
    mat U, V;
    vec S;
    uvec subsetRows = as<uvec>(subsetIndices) - 1;
    
    // Get the summary profile for the network subset from the SVD.
    bool success = svd_econ(U, S, V, aDat.rows(subsetRows), "right", "dc");
    if (!success) {
      Function warning("warning");
      warning("SVD failed to converge, does your data contain missing or"
              "infinite values?");
      return List::create(
        Named("membership") = NumericVector(1, NA_REAL),
        Named("propVarExpl") = NumericVector(1, NA_REAL)
      );
    }
    vec summary(V.col(1));

    // Flip the sign of the summary profile so that the eigenvector is 
    // positively correlated with the average scaled value of the underlying
    // data for the network subset.
    mat asDat((double *)xspDat->matrix(), xspDat->nrow(), xspDat->ncol(), false);
    mat ap = cor(mean(asDat.rows(subsetRows)), summary);
    if (ap(0,0) < 0) {                                                                                                                                                                                                                        
      for (unsigned int jj = 0; jj < xpDat->ncol(); jj++) {
        summary(jj) *= -1; 
      }   
    }   
    
    // We want the correlation between each variable (node) in the underlying
    // data and the summary profile for that network subset.
    mat p = cor(summary, aDat.rows(subsetRows).t());
    vec kSummary(p.t());
    // The proportion of variance explained is the sum of the squared 
    // correlation between the network subset summary profile, and each of the 
    // variables in the data that correspond to nodes in the network subset.
    vec pve(mean(square(p), 1));
    
    return List::create(
        Named("membership") = NumericVector(kSummary.begin(), kSummary.end()),
        Named("propVarExpl") = NumericVector(pve.begin(), pve.end())
      );
  } else {
    throw Rcpp::exception(
      "SVD can only be calculated on a big.matrix whose underlying type is"
      "'double'."
    );
  }
}