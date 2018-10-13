/*!
    Copyright (C) 2015-2018 Mateusz Łącki and Michał Startek.

    This file is part of IsoSpec.

    IsoSpec is free software: you can redistribute it and/or modify
    it under the terms of the Simplified ("2-clause") BSD licence.

    IsoSpec is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

    You should have received a copy of the Simplified BSD Licence
    along with IsoSpec.  If not, see <https://opensource.org/licenses/BSD-2-Clause>.
*/

#pragma once

#include <tuple>
#include <unordered_map>
#include <queue>
#include <limits>
#include "platform.h"
#include "dirtyAllocator.h"
#include "summator.h"
#include "operators.h"
#include "marginalTrek++.h"


#if ISOSPEC_BUILDING_R
#include <Rcpp.h>
using namespace Rcpp;
#endif /* ISOSPEC_BUILDING_R */


namespace IsoSpec
{

// This function is NOT guaranteed to be secure against malicious input. It should be used only for debugging.
unsigned int parse_formula(const char* formula,
                           std::vector<const double*>& isotope_masses,
                           std::vector<const double*>& isotope_probabilities,
                           int** isotopeNumbers,
                           int** atomCounts,
                           unsigned int* confSize);


//! The Iso class for the calculation of the isotopic distribution.
/*!
    It contains full description of the molecule for which one would like to calculate the isotopic distribution.
*/
class Iso {
private:

    //! Set up the marginal isotopic envelopes, corresponding to subisotopologues.
    /*!
        \param _isotopeMasses A table of masses of isotopes of the elements in the chemical formula,
                              e.g. {12.0, 13.003355, 1.007825, 2.014102} for C100H202.
        \param _isotopeProbabilities A table of isotope frequencies of the elements in the chemical formula,
                                     e.g. {.989212, .010788, .999885, .000115} for C100H202.
    */
    void setupMarginals(const double* const * _isotopeMasses,
                        const double* const * _isotopeProbabilities);
public:
    bool            disowned;       /*!< A variable showing if the Iso class was specialized by its child-class. If so, then the description of the molecules has been transferred there and Iso is a carcass class, dead as a dodo, an ex-class if you will. */
protected:
    int             dimNumber;      /*!< The number of elements in the chemical formula of the molecule. */
    int*            isotopeNumbers; /*!< A table with numbers of isotopes for each element. */
    int*            atomCounts;     /*!< A table with numbers of isotopes for each element. */
    unsigned int    confSize;       /*!< The number of bytes needed to represent the counts of isotopes present in the extended chemical formula. */
    int             allDim;         /*!< The total number of isotopes of elements present in a chemical formula, e.g. for H20 it is 2+3=5. */
    Marginal**      marginals;      /*!< The table of pointers to the distributions of individual subisotopologues. */
    double          modeLProb;      /*!< The log-probability of the mode of the isotopic distribution. */

public:
    //! General constructor.
    /*!
        \param _dimNumber The number of elements in the formula, e.g. for C100H202 it would be 2, as there are only carbon and hydrogen atoms.
        \param _isotopeNumbers A table with numbers of isotopes for each element, e.g. for C100H202 it would be {2, 2}, because both C and H have two stable isotopes.
        \param _atomCounts Number of atoms of each element in the formula, e.g. for C100H202 corresponds to {100, 202}.
        \param _isotopeMasses A table of masses of isotopes of the elements in the chemical formula, e.g. {12.0, 13.003355, 1.007825, 2.014102} for C100H202.
        \param _isotopeProbabilities A table of isotope frequencies of the elements in the chemical formula, e.g. {.989212, .010788, .999885, .000115} for C100H202.
    */
    Iso(
        int             _dimNumber,
        const int*      _isotopeNumbers,
        const int*      _atomCounts,
        const double* const *  _isotopeMasses,
        const double* const *  _isotopeProbabilities
    );

    //! Constructor from the formula object.
    Iso(const char* formula);

    //! The move constructor.
    Iso(Iso&& other);

    //! The copy constructor.
    /*!
        \param other The other instance of the Iso class.
        \param fullcopy If false, copy only the number of atoms in the formula, the size of the configuration, the total number of isotopes, and the probability of the mode isotopologue.
    */
    Iso(const Iso& other, bool fullcopy);

    //! Destructor.
    virtual ~Iso();

    //! Get the mass of the lightest peak in the isotopic distribution.
    double getLightestPeakMass() const;

    //! Get the mass of the heaviest peak in the isotopic distribution.
    double getHeaviestPeakMass() const;

    //! Get the log-probability of the mode-configuration (if there are many modes, they share this value).
    inline double getModeLProb() const { return modeLProb; };

    //! Get the number of elements in the chemical formula of the molecule.
    inline int getDimNumber() const { return dimNumber; };

    //! Get the total number of isotopes of elements present in a chemical formula.
    inline int getAllDim() const { return allDim; };

    //! Get the marginal distributions of subisotopologues.
    /*!
        \param Lcutoff The logarithm of the cut off value.
        \param absolute Should the cutoff be in terms of absolute height of the peak, or relative to the height/probability of the mode.
        \param tabSize The size of the extension of the table with configurations.
        \param hashSize The size of the hash-table used to store subisotopologues and check if they have been already calculated.
    */
    PrecalculatedMarginal** get_MT_marginal_set(double Lcutoff, bool absolute, int tabSize, int hashSize);
};

// Be very absolutely safe vs. false-sharing cache lines between threads...
#define ISOSPEC_PADDING 64

// Note: __GNUC__ is defined by clang and gcc
#ifdef __GNUC__
#define LIKELY(condition) __builtin_expect(static_cast<bool>(condition), 1)
#define UNLIKELY(condition) __builtin_expect(static_cast<bool>(condition), 0)
// For aggressive inlining 
#define INLINE __attribute__ ((always_inline)) inline
#elif defined _MSC_VER
#define LIKELY(condition) condition
#define UNLIKELY(condition) condition
#define INLINE __forceinline inline
#else
#define LIKELY(condition) condition
#define UNLIKELY(condition) condition
#define INLINE inline
#endif
    
//! The generator of isotopologues.
/*!
    This class provides the common interface for all isotopic generators.
*/
class IsoGenerator : public Iso
{
protected:
    double* partialLProbs;  /*!< The prefix sum of the log-probabilities of the current isotopologue. */
    double* partialMasses;  /*!< The prefix sum of the masses of the current isotopologue. */
    double* partialExpProbs;/*!< The prefix product of the probabilities of the current isotopologue. */

public:
    //! Advance to the next, not yet visited, most probable isotopologue.
    /*!
        \return Return false if it is not possible to advance.
    */
    virtual bool advanceToNextConfiguration() = 0;

    //! Get the log-probability of the current isotopologue.
    /*!
        \return The log-probability of the current isotopologue.
    */
    inline double lprob() const { return partialLProbs[0]; };

    //! Get the mass of the current isotopologue.
    /*!
        \return The mass of the current isotopologue.
    */
    inline double mass()  const { return partialMasses[0]; };

    //! Get the probability of the current isotopologue.
    /*!
        \return The probability of the current isotopologue.
    */
    inline double eprob() const { return partialExpProbs[0]; };

    //! Save the counts of isotopes in the space.
    /*!
        \param space An array where counts of isotopes shall be written. 
                     Must be as big as the overall number of isotopes.
    */
    virtual void get_conf_signature(int* space) const = 0;

    //! Move constructor.
    IsoGenerator(Iso&& iso, bool alloc_partials = true);

    //! Destructor.
    virtual ~IsoGenerator();
};



//! The generator of isotopologues sorted by their probability of occurrence.
/*!
    The subsequent isotopologues are generated with diminishing probability, starting from the mode.
    This algorithm take O(N*log(N)) to compute the N isotopologues because of using the Priority Queue data structure.
    Obtaining the N isotopologues can be achieved in O(N) if they are not required to be spit out in the descending order.
*/
class IsoOrderedGenerator: public IsoGenerator
{
private:
    MarginalTrek**              marginalResults;            /*!< Table of pointers to marginal distributions of subisotopologues. */
    std::priority_queue<void*,std::vector<void*>,ConfOrder> pq; /*!< The priority queue used to generate isotopologues ordered by descending probability. */
    void*                       topConf;                    /*!< Most probable configuration. */
    DirtyAllocator              allocator;                  /*!< Structure used for allocating memory for isotopologues. */
    const std::vector<double>** logProbs;                   /*!< Obtained log-probabilities. */
    const std::vector<double>** masses;                     /*!< Obtained masses. */
    const std::vector<int*>**   marginalConfs;              /*!< Obtained counts of isotopes. */
    double                      currentLProb;               /*!< The log-probability of the current isotopologue. */
    double                      currentMass;                /*!< The mass of the current isotopologue. */
    double                      currentEProb;               /*!< The probability of the current isotopologue. */
    int                         ccount;

public:
    bool advanceToNextConfiguration() override final;
    inline void get_conf_signature(int* space) const override final
    {
        int* c = getConf(topConf);

        if (ccount >= 0)
            c[ccount]--;

        for(int ii=0; ii<dimNumber; ii++)
        {
            memcpy(space, marginalResults[ii]->confs()[c[ii]], isotopeNumbers[ii]*sizeof(int));
            space += isotopeNumbers[ii];
        }

        if (ccount >= 0)
            c[ccount]++;
    };

    //! The move-constructor.
    IsoOrderedGenerator(Iso&& iso, int _tabSize  = 1000, int _hashSize = 1000);

    //! Destructor.
    virtual ~IsoOrderedGenerator();
};



//! The generator of isotopologues above a given threshold value.
/*!
    Attention: the calculated configurations are only partially ordered and the user should not assume they will be ordered.
    This algorithm computes N isotopologues in O(N) thanks to storing data in FIFO Queue.
    It is a considerable advantage w.r.t. the IsoOrderedGenerator.
*/
class IsoThresholdGenerator: public IsoGenerator
{
protected:
    int*                    counter;            /*!< An array storing the position of an isotopologue in terms of the subisotopologues ordered by decreasing probability. */
    double*                 maxConfsLPSum;
    const double            Lcutoff;            /*!< The logarithm of the lower bound on the calculated probabilities. */
    PrecalculatedMarginal** marginalResults;

public:
    bool advanceToNextConfiguration() override;
    inline void get_conf_signature(int* space) const override
    {
        for(int ii=0; ii<dimNumber; ii++)
        {
            memcpy(space, marginalResults[ii]->get_conf(counter[ii]), isotopeNumbers[ii]*sizeof(int));
            space += isotopeNumbers[ii];
        }
    };

    //! The move-constructor.
    /*!
        \param iso An instance of the Iso class.
        \param _threshold The threshold value.
        \param _absolute If true, the _threshold is interpreted as the absolute minimal peak height for the isotopologues.
                         If false, the _threshold is the fraction of the highest peak's probability.
        \param tabSize The size of the extension of the table with configurations.
        \param hashSize The size of the hash-table used to store subisotopologues and check if they have been already calculated.
    */
    IsoThresholdGenerator(Iso&& iso, double _threshold, bool _absolute=true,
                        int _tabSize=1000, int _hashSize=1000);

    //! Destructor.
    inline virtual ~IsoThresholdGenerator() { delete[] counter;
                                              delete[] maxConfsLPSum;
                                              dealloc_table(marginalResults, dimNumber); };

    //! Block the subsequent search of isotopologues.
    void terminate_search();

private:
    //! Recalculate the current partial log-probabilities, masses, and probabilities.
    inline void recalc(int idx)
    {
        for(; idx >=0; idx--)
        {
            partialLProbs[idx] = partialLProbs[idx+1] + marginalResults[idx]->get_lProb(counter[idx]);
            partialMasses[idx] = partialMasses[idx+1] + marginalResults[idx]->get_mass(counter[idx]);
            partialExpProbs[idx] = partialExpProbs[idx+1] * marginalResults[idx]->get_eProb(counter[idx]);
        }
    }


};

/** This is a generator class for fast generation of isotopic probabilities.
*/
class IsoThresholdGeneratorFast: public IsoThresholdGenerator
{
  protected:

    const double* lProbs_ptr;
    const double* mass_ptr;
    const double* exp_ptr;
    double* partialLProbs_first;
    double* partialLProbs_second;
    double* partialMasses_first;
    double* partialMasses_second;
    double* partialeProbs_first;
    double* partialeProbs_second;
    int* counter_first;

public:
    virtual void get_conf_signature(int*) const {};

    IsoThresholdGeneratorFast(Iso&& iso, double _threshold, bool _absolute=true,
                        int _tabSize=1000, int _hashSize=1000) :
      IsoThresholdGenerator(std::move(iso), _threshold, _absolute, _tabSize, _hashSize)
    {
      lProbs_ptr = marginalResults[0]->get_lProbs_ptr();
      mass_ptr = marginalResults[0]->get_masses_ptr();
      exp_ptr = marginalResults[0]->get_eProbs_ptr();

      counter_first = counter;
      partialLProbs_first = partialLProbs;
      partialLProbs_second = partialLProbs;
      partialLProbs_second++;

      partialeProbs_first = partialExpProbs;
      partialeProbs_second = partialExpProbs;
      partialeProbs_second++;

      partialMasses_first = partialMasses;
      partialMasses_second = partialMasses;
      partialMasses_second++;
    }

    // Perform highly aggressive inling as this function is often called as while(advanceToNextConfiguration()) {}
    // which leads to an extremely tight loop and some compilers miss this (potentially due to the length of the function). 
    INLINE bool advanceToNextConfiguration()
    {
        (*counter_first)++; // counter[0]++;
        *partialLProbs_first = *partialLProbs_second + *lProbs_ptr;
        lProbs_ptr++;
        mass_ptr++;
        exp_ptr++;
        if(LIKELY(*partialLProbs_first >= Lcutoff))
        {
            *partialMasses_first = *partialMasses_second + *mass_ptr;
            *partialeProbs_first = *partialeProbs_second * (*exp_ptr);
            return true;
        }

        // If we reached this point, a carry is needed

        int idx = 0;
        lProbs_ptr = marginalResults[0]->get_lProbs_ptr();
        mass_ptr = marginalResults[0]->get_masses_ptr();
        exp_ptr = marginalResults[0]->get_eProbs_ptr();
        lProbs_ptr++;
        mass_ptr++;
        exp_ptr++;

        int * cntr_ptr = counter;

        while(idx<dimNumber-1)
        {
            // counter[idx] = 0;
            *cntr_ptr = 0;
            idx++;
            cntr_ptr++;
            // counter[idx]++;
            (*cntr_ptr)++;
            partialLProbs[idx] = partialLProbs[idx+1] + marginalResults[idx]->get_lProb(counter[idx]);
            if(LIKELY(partialLProbs[idx] + maxConfsLPSum[idx-1] >= Lcutoff)) // this likely really helps gcc
            {
                partialMasses[idx] = partialMasses[idx+1] + marginalResults[idx]->get_mass(counter[idx]);
                partialExpProbs[idx] = partialExpProbs[idx+1] * marginalResults[idx]->get_eProb(counter[idx]);
                recalc(idx-1);
                return true;
            }
        }

        terminate_search();
        return false;
    }

private:
    INLINE void recalc(int idx)
    {
        for(; idx >=0; idx--)
        {
            partialLProbs[idx] = partialLProbs[idx+1] + marginalResults[idx]->get_lProb(counter[idx]);
            partialMasses[idx] = partialMasses[idx+1] + marginalResults[idx]->get_mass(counter[idx]);
            partialExpProbs[idx] = partialExpProbs[idx+1] * marginalResults[idx]->get_eProb(counter[idx]);
        }
    }
};

/** This is a generator class for counting the number of configurations only,
 * it will not produce the masses or the exponential probabilities.
*/
class IsoThresholdGeneratorCntr: public IsoThresholdGeneratorFast
{

  private:
    inline double mass();
    inline double eprob();
    virtual void get_conf_signature(int*) const {};
public:

    IsoThresholdGeneratorCntr(Iso&& iso, double _threshold, bool _absolute=true,
                        int _tabSize=1000, int _hashSize=1000) :
      IsoThresholdGeneratorFast(std::move(iso), _threshold, _absolute, _tabSize, _hashSize)
    {
    }
    
    // Perform highly aggressive inling as this function is often called as while(advanceToNextConfiguration()) {}
    // which leads to an extremely tight loop and some compilers miss this (potentially due to the length of the function). 
    INLINE bool advanceToNextConfiguration()
    {
        (*counter_first)++; // counter[0]++;
        *partialLProbs_first = *partialLProbs_second + *lProbs_ptr;
        lProbs_ptr++;
        if(LIKELY(*partialLProbs_first >= Lcutoff))
        {
            return true;
        }

        // If we reached this point, a carry is needed

        int idx = 0;
        lProbs_ptr = marginalResults[0]->get_lProbs_ptr();
        lProbs_ptr++;

        int * cntr_ptr = counter;

        while(idx<dimNumber-1)
        {
            *cntr_ptr = 0;
            idx++;
            cntr_ptr++;
            (*cntr_ptr)++;
            partialLProbs[idx] = partialLProbs[idx+1] + marginalResults[idx]->get_lProb(counter[idx]);
            if(partialLProbs[idx] + maxConfsLPSum[idx-1] >= Lcutoff)
            {
                recalc(idx-1);
                return true;
            }
        }

        terminate_search();
        return false;
    }

private:
    INLINE void recalc(int idx)
    {
        for(; idx >=0; idx--)
        {
            partialLProbs[idx] = partialLProbs[idx+1] + marginalResults[idx]->get_lProb(counter[idx]);
        }
    }

};

//! The multi-threaded version of the generator of isotopologues.
/*!
    Attention: this code is experimental.
*/
class IsoThresholdGeneratorMT : public IsoGenerator
{
private:
    unsigned int* counter;
    double* maxConfsLPSum;
    const double Lcutoff;
    SyncMarginal* last_marginal;
    PrecalculatedMarginal** marginalResults;

public:
    bool advanceToNextConfiguration() override final;
    inline void get_conf_signature(int* space) const override final
    {
        for(int ii=0; ii<dimNumber; ii++)
        {
            memcpy(space, marginalResults[ii]->get_conf(counter[ii]), isotopeNumbers[ii]*sizeof(int));
            space += isotopeNumbers[ii];
        }
    };

    //! Move constructor.
    IsoThresholdGeneratorMT(Iso&& iso, double  _threshold, PrecalculatedMarginal** marginals, bool _absolute = true);

    //! Destructor.
    inline virtual ~IsoThresholdGeneratorMT() { delete[] counter; delete[] maxConfsLPSum;};
    
    //! Block the subsequent search of isotopologues.
    void terminate_search();

private:
    inline void recalc(int idx)
    {
        for(; idx >=0; idx--)
        {
            partialLProbs[idx] = partialLProbs[idx+1] + marginalResults[idx]->get_lProb(counter[idx]);
            partialMasses[idx] = partialMasses[idx+1] + marginalResults[idx]->get_mass(counter[idx]);
            partialExpProbs[idx] = partialExpProbs[idx+1] * marginalResults[idx]->get_eProb(counter[idx]);
        }
    }



};



//! The generator of isotopologues above a given joint probability value.
/*!
    This class generates subsequent isotopologues that ARE NOT GUARANTEED TO BE ORDERED BY probability.
    The overal set of isotopologues is guaranteed to surpass a given threshold of probability contained in the
    isotopic distribution.
    This calculations are performed in O(N) operations, where N is the total number of the output isotopologues.
*/
class IsoLayeredGenerator : public IsoGenerator
{
private:
    int* counter;
    double* maxConfsLPSum;
    double last_layer_lcutoff, current_layer_lcutoff;
    Summator current_sum;
    LayeredMarginal** marginalResults;
    double* probsExcept;
    int* last_counters;
    double delta;
    double final_cutoff;

public:
    bool advanceToNextConfiguration_internal();

    //! Calculate a new layer of isotopologues.
    /*!
        \param _delta The new difference between the old and the new threshold on the minimal log-probability.
    */
    inline void get_next_isotopologues_layer(double _delta) { delta = _delta; nextLayer(delta); };

    inline bool advanceToNextConfiguration() override final
    {
        while (!advanceToNextConfiguration_internal())
            if (!nextLayer(delta))
                return false;
        std::cout << "Returning conf: " << counter[0] << " " << counter[1] << " " << partialLProbs[0] << std::endl;
        return true;
    }

    bool nextLayer(double logCutoff_delta); // Arg should be negative

    IsoLayeredGenerator(Iso&& iso, double _delta = -3.0, int _tabSize  = 1000, int _hashSize = 1000);

    inline void get_conf_signature(int* space) const override final
    {
        for(int ii=0; ii<dimNumber; ii++)
        {
            memcpy(space, marginalResults[ii]->get_conf(counter[ii]), isotopeNumbers[ii]*sizeof(int));
            space += isotopeNumbers[ii];
        }
    };

    //! Destructor.
    virtual ~IsoLayeredGenerator();

    //! Block the subsequent search of isotopologues.
    void terminate_search();


private:
    inline void recalc(int idx)
    {
        for(; idx >=0; idx--)
        {
            partialLProbs[idx] = partialLProbs[idx+1] + marginalResults[idx]->get_lProb(counter[idx]);
            partialMasses[idx] = partialMasses[idx+1] + marginalResults[idx]->get_mass(counter[idx]);
            partialExpProbs[idx] = partialExpProbs[idx+1] * marginalResults[idx]->get_eProb(counter[idx]);
        }
    }
};




#if !ISOSPEC_BUILDING_R

void printConfigurations(
    const   std::tuple<double*,double*,int*,int>& results,
    int     dimNumber,
    int*    isotopeNumbers
);
#endif /* !ISOSPEC_BUILDING_R */

} // namespace IsoSpec

