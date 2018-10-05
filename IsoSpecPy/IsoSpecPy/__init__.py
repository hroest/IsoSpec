# -*- coding: utf-8 -*-
#
#   Copyright (C) 2015-2016 Mateusz Łącki and Michał Startek.
#
#   This file is part of IsoSpec.
#
#   IsoSpec is free software: you can redistribute it and/or modify
#   it under the terms of the Simplified ("2-clause") BSD licence.
#
#   IsoSpec is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
#
#   You should have received a copy of the Simplified BSD Licence
#   along with IsoSpec.  If not, see <https://opensource.org/licenses/BSD-2-Clause>.
# 

from .isoFFI import isoFFI
import re
import types
from . import PeriodicTbl
from .confs_passthrough import ConfsPassthrough


try:
    xrange
except NameError:
    xrange = range

regex_pattern = re.compile('([A-Z][a-z]?)([0-9]*)')

def IsoParamsFromFormula(formula):
    global regex_pattern

    symbols = []
    atomCounts = []
    for elem, cnt in re.findall(regex_pattern, formula):
        symbols.append(elem)
        atomCounts.append(int(cnt) if cnt is not '' else 1)
    try:
        masses = tuple(PeriodicTbl.symbol_to_masses[s] for s in symbols)
        probs  = tuple(PeriodicTbl.symbol_to_probs[s]  for s in symbols)
    except KeyError:
        raise ValueError("Invalid formula")

    return (atomCounts, masses, probs)



class Iso(object):
    def __init__(self, formula=None,
                 get_confs=False,
                 atomCounts=None,
                 isotopeMasses=None,
                 isotopeProbabilities=None):
        """Initialize Iso."""

        self.iso = None

        if formula is None and not all([atomCounts, isotopeMasses, isotopeProbabilities]):
            raise Exception("Either formula or ALL of: atomCounts, isotopeMasses, isotopeProbabilities must not be None")

        if formula is not None:
            self.atomCounts, self.isotopeMasses, self.isotopeProbabilities = IsoParamsFromFormula(formula)

        if atomCounts is not None:
            self.atomCounts = atomCounts


        if isotopeMasses is not None:
            self.isotopeMasses = isotopeMasses

        if isotopeProbabilities is not None:
            self.isotopeProbabilities = isotopeProbabilities
        
        self.isotopeNumbers = tuple(map(len, self.isotopeMasses))
        assert self.isotopeNumbers == tuple(map(len, self.isotopeProbabilities))

        self.dimNumber = len(self.isotopeNumbers)

        self.get_confs = get_confs
        self.ffi = isoFFI.clib

        offsets = []

        if get_confs:
            i = 0
            for j in xrange(self.dimNumber):
                newl = []
                for k in xrange(self.isotopeNumbers[j]):
                    newl.append(i)
                    i += 1
                offsets.append(tuple(newl))
            self.offsets = tuple(offsets)

        self.iso = self.ffi.setupIso(self.dimNumber, self.isotopeNumbers,
                                     self.atomCounts,
                                     [i for s in self.isotopeMasses for i in s],
                                     [i for s in self.isotopeProbabilities for i in s])

    def __iter__(self):
        if self.get_confs:
            raise NotImplementedError() # TODO: implement
        else:
            for i in xrange(self.size):
                yield (self.masses[i], self.probs[i])
                
    def __del__(self):
        if self.iso is not None:
            self.ffi.deleteIso(self.iso)
        

    def parse_conf(self, cptr, starting_with = 0):
        return tuple(tuple(cptr[i+starting_with] for i in o) for o in self.offsets)



class IsoThreshold(Iso):
    def __init__(self, threshold, absolute=False, get_confs = False, **kwargs):
        self.tabulator = None
        self.generator = None
        super(IsoThreshold, self).__init__(get_confs = get_confs, **kwargs)
        self.threshold = threshold
        self.absolute = absolute

        self.generator = self.ffi.setupIsoThresholdGenerator(self.iso, threshold, absolute, 1000, 1000)
        self.tabulator = self.ffi.setupThresholdTabulator(self.generator, True, True, True, get_confs)

        self.size = self.ffi.confs_noThresholdTabulator(self.tabulator)

        def c(typename, what, mult = 1):
            return isoFFI.ffi.cast(typename + '[' + str(self.size*mult) + ']', what)

        self.masses = c("double", self.ffi.massesThresholdTabulator(self.tabulator))
        self.lprobs = c("double", self.ffi.lprobsThresholdTabulator(self.tabulator))
        self.probs  = c("double", self.ffi.probsThresholdTabulator(self.tabulator))

        if get_confs:
            self.sum_isotope_numbers = sum(self.isotopeNumbers)
            self.raw_confs = c("int", self.ffi.confsThresholdTabulator(self.tabulator), mult = self.sum_isotope_numbers)
            self.confs = ConfsPassthrough(lambda idx: self._get_conf(idx), self.size)


    def _get_conf(self, idx):
        return self.parse_conf(self.raw_confs, starting_with = self.sum_isotope_numbers * idx)

    def __len__(self):
        return self.size

    def __del__(self):
        if self.tabulator is not None:
            self.ffi.deleteThresholdTabulator(self.tabulator)
        if self.generator is not None:
            self.ffi.deleteIsoThresholdGenerator(self.generator)




'''

class IsoLayered(Iso):
    def __init__(self, delta=-10.0, **kwargs):
        super(IsoLayered, self).__init__(**kwargs)
        self.delta = delta

    def __iter__(self):
        return IsoLayeredGenerator(src=self, get_confs=self.get_confs)

    def __del__(self):
        pass

'''

class IsoGenerator(Iso):
    def __init__(self, get_confs=False, **kwargs):
        self.cgen = None
        super(IsoGenerator, self).__init__(get_confs = get_confs, **kwargs)
        self.conf_space = isoFFI.ffi.new("int[" + str(sum(self.isotopeNumbers)) + "]")
        self.firstuse = True

    def __iter__(self):
        if not self.firstuse:
            raise NotImplementedError("Multiple iterations through the same IsoGenerator object are not supported. Either create a new (identical) generator for a second loop-through, or use one of the non-generator classes, which do support being re-used.")
        self.firstuse = False
        cgen = self.cgen
        if self.get_confs:
            while self.advancer(cgen):
                self.conf_getter(cgen, self.conf_space)
                yield (self.mass_getter(cgen), self.lprob_getter(cgen), self.parse_conf(self.conf_space))
        else:
            while self.advancer(cgen):
                yield (self.mass_getter(cgen), self.lprob_getter(cgen))

        

class IsoThresholdGenerator(IsoGenerator):
    def __init__(self, threshold, absolute=False, get_confs=False, **kwargs):
        super(IsoThresholdGenerator, self).__init__(get_confs, **kwargs)
        self.threshold = threshold
        self.absolute = absolute
        self.cgen = self.ffi.setupIsoThresholdGenerator(self.iso,
                                                        threshold,
                                                        absolute,
                                                        1000,
                                                        1000)
        self.advancer = self.ffi.advanceToNextConfigurationIsoThresholdGenerator
        self.lprob_getter = self.ffi.lprobIsoThresholdGenerator
        self.mass_getter = self.ffi.massIsoThresholdGenerator
        self.conf_getter = self.ffi.get_conf_signatureIsoThresholdGenerator

    def __del__(self):
        if self.cgen is not None:
            self.ffi.deleteIsoThresholdGenerator(self.cgen)


class IsoLayeredGenerator(IsoGenerator):
    def __init__(self, delta = -10.0, get_confs=False, **kwargs):
        assert delta < 0.0
        super(IsoLayeredGenerator, self).__init__(get_confs, **kwargs)
        self.delta = delta
        self.cgen = self.ffi.setupIsoLayeredGenerator(self.iso,
                                                      self.delta,
                                                      1000,
                                                      1000)
        self.advancer = self.ffi.advanceToNextConfigurationIsoLayeredGenerator
        self.lprob_getter = self.ffi.lprobIsoLayeredGenerator
        self.mass_getter = self.ffi.massIsoLayeredGenerator
        self.conf_getter = self.ffi.get_conf_signatureIsoLayeredGenerator

    def __del__(self):
        if self.cgen is not None:
            self.ffi.deleteIsoLayeredGenerator(self.cgen)

class IsoOrderedGenerator(IsoGenerator):
    def __init__(self, get_confs=False, **kwargs):
        super(IsoOrderedGenerator, self).__init__(get_confs, **kwargs)
        self.cgen = self.ffi.setupIsoOrderedGenerator(self.iso,
                                                      1000,
                                                      1000)
        self.advancer = self.ffi.advanceToNextConfigurationIsoOrderedGenerator
        self.lprob_getter = self.ffi.lprobIsoOrderedGenerator
        self.mass_getter = self.ffi.massIsoOrderedGenerator
        self.conf_getter = self.ffi.get_conf_signatureIsoOrderedGenerator

    def __del__(self):
        if self.cgen is not None:
            self.ffi.deleteIsoLayeredGenerator(self.cgen)




__version__ = "1.9.0a2"


# For compatibility with 1.0.X

class CompatIsoWrapper(object):
    def __init__(self):
        from .IsoSpecPy import IsoSpec, IsoSpecify, IsoPlot
        self.IsoSpec = IsoSpec
        self.IsoSpecify = IsoSpecify
        self.IsoPlot = IsoPlot


IsoSpecPy = CompatIsoWrapper()

