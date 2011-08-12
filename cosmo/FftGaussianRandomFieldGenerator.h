// Created 12-Aug-2011 by David Kirkby (University of California, Irvine) <dkirkby@uci.edu>

#ifndef COSMO_FFT_GAUSSIAN_RANDOM_FIELD_GENERATOR
#define COSMO_FFT_GAUSSIAN_RANDOM_FIELD_GENERATOR

#include "cosmo/AbsGaussianRandomFieldGenerator.h"

namespace cosmo {
    // Implements the abstract Gaussian random field generator interface using FFT.
	class FftGaussianRandomFieldGenerator : public AbsGaussianRandomFieldGenerator {
	public:
		FftGaussianRandomFieldGenerator(PowerSpectrumPtr powerSpectrum, double spacing,
		    int nx, int ny, int nz);
		virtual ~FftGaussianRandomFieldGenerator();
	private:
        // The generate() method calls this virtual method.
        virtual void _generate(int seed);
        // The getField method calls this virtual method after checking that data is
        // available and that (x,y,z) are valid values.
        virtual double _getFieldUnchecked(int x, int y, int z) const;
	}; // FftGaussianRandomFieldGenerator
} // cosmo

#endif // COSMO_FFT_GAUSSIAN_RANDOM_FIELD_GENERATOR