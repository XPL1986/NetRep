#define ARMA_USE_LAPACK
#define ARMA_NO_DEBUG

#include <RcppArmadillo.h>
using namespace Rcpp;
using namespace arma;

// [[Rcpp::depends(BH, bigmemory, RcppArmadillo)]]
#include <bigmemory/BigMatrix.h>

//' Network subset eigenvector and proportion of variance explained in C++
//' 
//' @param pDat SEXP container for the pointer to a scaled version of the 
//'   data matrix used to construct the network.
//' @param subsetIndices indices of the network subset of interest in 
//'   \code{pDat}.
//' 
//' @return
//'  A list containing:
//'  \enumerate{
//'   \item{\emph{"SEP"}:}{
//'     The Summary Expression Profile of each node (see details).
//'   }
//'   \item{\emph{"MM"}:}{
//'     The Module Membership of each node (see details).
//'   }
//'   \item{\emph{"pve"}:}{
//'     The proportion of the variance explained by the subset's summary 
//'     expression profile (see details).
//'   }
//'  }
//'  
//' @references
//'  \enumerate{
//'     \item{
//'       Langfelder, P., Luo, R., Oldham, M. C. & Horvath, S. \emph{Is my
//'       network module preserved and reproducible?} PLoS Comput. Biol. 
//'       \strong{7}, e1001057 (2011). 
//'     }
//'  }
//'  
//' @details
//'  First, a summar expression profile (SEP) is calculated for the 
//'  module from the underlying gene expression data. This corresponds to the 
//'  first eigenvector of a principal component analysis \emph{(1)}. 
//'  
//'  The orientation of the eigenvector is modified so that its sign is in the
//'  same direction as the gene expression (on average).
//'  
//'  The Module Membership (MM) is thus quantified as the correlation between each
//'  gene in the module and the summary expression profile.
//'  
//'  The proportion of variance in the module's gene expression data explained 
//'  by the summary expression profile (pve) is quantified as the average square
//' of the Module Membership \emph{(1)}.
//' 
//' @import RcppArmadillo
//' @rdname dataProps-cpp
//'  
// [[Rcpp::export]]
List DataProps(
  SEXP pDat, IntegerVector subsetIndices
) {
  XPtr<BigMatrix> xpDat(pDat);
  
  // Make sure we're not indexing out of range.
  if (is_true(any(subsetIndices <= 0)) || 
      is_true(any(subsetIndices > xpDat->ncol()))) {
    throw std::out_of_range("Some of requested indices are outside of range!");
  }
  
  // We can only work with BigMatrix objects of type double here due to SVD 
  // requirements.
  if (xpDat->matrix_type() == 8) {
    // Cast the BigMatrix to an arma::Mat<double>
    mat aDat((double *)xpDat->matrix(), xpDat->nrow(), xpDat->ncol(), false);
    mat U, V;
    vec S;
    uvec subsetCols = sort(as<uvec>(subsetIndices) - 1);
    
    // Get the summary profile for the network subset from the SVD.
    bool success = svd_econ(U, S, V, aDat.cols(subsetCols), "left", "dc");
    if (!success) {
      Function warning("warning");
      warning("SVD failed to converge, does your data contain missing or"
              " infinite values?");
      return List::create(
          Named("SEP") = NA_REAL,
          Named("MM") = NA_REAL,
          Named("pve") = NA_REAL
        );
    }
    mat summary(U.col(0));

    // Flip the sign of the summary profile so that the eigenvector is 
    // positively correlated with the average scaled value of the underlying
    // data for the network subset.
    mat ap = cor(mean(aDat.cols(subsetCols), 1), summary);
    if (ap(0,0) < 0) {
      summary *= -1; 
    }
    
    // We want the correlation between each variable (node) in the underlying
    // data and the summary profile for that network subset.
    mat p = cor(summary, aDat.cols(subsetCols));
    mat MM(p);
    
    // To make sure the resulting MAR and KIM vectors are in the correct order,
    // order the results to match the original ordering of subsetIndices.
    Function rank("rank"); // Rank only works on R objects like IntegerVector.
    uvec idxRank = as<uvec>(rank(subsetIndices)) - 1;

    vec oMM = MM(idxRank);

    // The proportion of variance explained is the sum of the squared 
    // correlation between the network subset summary profile, and each of the 
    // variables in the data that correspond to nodes in the network subset.
    mat pve(mean(square(p), 1));
    
    return List::create(
        Named("SEP") = summary,
        Named("MM") = oMM,
        Named("pve") = pve
      );
  } else {
    throw Rcpp::exception(
      "SVD can only be calculated on a big.matrix whose underlying type is"
      "'double'."
    );
  }
}