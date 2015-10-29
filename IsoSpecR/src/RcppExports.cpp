// This file was generated by Rcpp::compileAttributes
// Generator token: 10BE3573-1514-4C36-9D1C-5A225CD40393

#include <Rcpp.h>

using namespace Rcpp;

// Rinterface
List Rinterface(IntegerVector isotopeNumbers, IntegerVector atomCounts, NumericVector isotopeMasses, NumericVector isotopeProbabilities, double stopCondition, int algo, int tabSize, int hashSize, double step);
RcppExport SEXP IsoSpecR_Rinterface(SEXP isotopeNumbersSEXP, SEXP atomCountsSEXP, SEXP isotopeMassesSEXP, SEXP isotopeProbabilitiesSEXP, SEXP stopConditionSEXP, SEXP algoSEXP, SEXP tabSizeSEXP, SEXP hashSizeSEXP, SEXP stepSEXP) {
BEGIN_RCPP
    Rcpp::RObject __result;
    Rcpp::RNGScope __rngScope;
    Rcpp::traits::input_parameter< IntegerVector >::type isotopeNumbers(isotopeNumbersSEXP);
    Rcpp::traits::input_parameter< IntegerVector >::type atomCounts(atomCountsSEXP);
    Rcpp::traits::input_parameter< NumericVector >::type isotopeMasses(isotopeMassesSEXP);
    Rcpp::traits::input_parameter< NumericVector >::type isotopeProbabilities(isotopeProbabilitiesSEXP);
    Rcpp::traits::input_parameter< double >::type stopCondition(stopConditionSEXP);
    Rcpp::traits::input_parameter< int >::type algo(algoSEXP);
    Rcpp::traits::input_parameter< int >::type tabSize(tabSizeSEXP);
    Rcpp::traits::input_parameter< int >::type hashSize(hashSizeSEXP);
    Rcpp::traits::input_parameter< double >::type step(stepSEXP);
    __result = Rcpp::wrap(Rinterface(isotopeNumbers, atomCounts, isotopeMasses, isotopeProbabilities, stopCondition, algo, tabSize, hashSize, step));
    return __result;
END_RCPP
}
