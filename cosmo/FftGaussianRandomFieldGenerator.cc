// Created 12-Aug-2011 by David Kirkby (University of California, Irvine) <dkirkby@uci.edu>

#include "cosmo/FftGaussianRandomFieldGenerator.h"
#include "cosmo/RuntimeError.h"

#include "likely/Random.h"
#include "likely/WeightedAccumulator.h"

#include "config.h"
#ifdef HAVE_LIBFFTW3F
#include "fftw3.h"
//#define FFTW(X) fftw_ ## X // double transforms
#define FFTW(X) fftwf_ ## X // float transforms
//#define FFTW(X) fftwl_ ## X // long double transforms
typedef float FftwReal;
#endif

namespace local = cosmo;

namespace cosmo {
    struct FftGaussianRandomFieldGenerator::Implementation {
#ifdef HAVE_LIBFFTW3F
        FFTW(complex) *data;
        FFTW(plan) plan;
#endif
    };
} // cosmo::

local::FftGaussianRandomFieldGenerator::FftGaussianRandomFieldGenerator(
PowerSpectrumPtr powerSpectrum, double spacing, int nx, int ny, int nz, likely::RandomPtr random)
: AbsGaussianRandomFieldGenerator(powerSpectrum,spacing,nx,ny,nz,random),
_pimpl(new Implementation()), _halfz(nz/2+1)
{
#ifdef HAVE_LIBFFTW3F
    _pimpl->data = 0;
#else
    throw RuntimeError("FftGaussianRandomFieldGenerator: package not built with FFTW3.");
#endif
    // Calculate the number of complex values needed in k space.
    _nbuf = (std::size_t)getNx()*getNy()*_halfz;
}

local::FftGaussianRandomFieldGenerator::~FftGaussianRandomFieldGenerator() {
#ifdef HAVE_LIBFFTW3F
    if(0 != _pimpl->data) {
        FFTW(destroy_plan)(_pimpl->plan);
    }
#endif
}

void local::FftGaussianRandomFieldGenerator::generateFieldK() {
#ifdef HAVE_LIBFFTW3F
    // Cleanup any previous plan.
    if(_pimpl->data) FFTW(destroy_plan)(_pimpl->plan);
    // Generate random (real,imag) components with unit Gaussian distributions.
    std::size_t ngen(2*_nbuf);
    _buffer = getRandom()->fillFloatArrayNormal(ngen);
    // Create a new plan for the new buffer.
    _pimpl->data = (FFTW(complex)*)&_buffer[0];
    FftwReal *realData = (FftwReal*)(_pimpl->data);
    _pimpl->plan = FFTW(plan_dft_c2r_3d)(getNx(),getNy(),getNz(),_pimpl->data,realData,FFTW_ESTIMATE);
    // Scale each complex value according to the power for the coresponding k-vector.
    double twopi(8*std::atan(1)), spacing(getSpacing());
    double dkx = twopi/(getNx()*spacing), dky = twopi/(getNy()*spacing), dkz = twopi/(getNz()*spacing);
    double dk3 = dkx*dky*dkz/(2*twopi);
    int nxby2 = getNx()/2, nyby2 = getNy()/2, nzby2 = getNz()/2;
    for(int ix = 0; ix < getNx(); ++ix) {
        double kx = (ix > nxby2 ? ix-getNx() : ix)*dkx;
        for(int iy = 0; iy < getNy(); ++iy) {
            double ky = (iy > nyby2 ? iy-getNy() : iy)*dky;
            for(int iz = 0; iz < _halfz; ++iz) {
                double kz = iz*dkz;
                double ksq = kx*kx + ky*ky + kz*kz;
                double sigma(0);
                if(ksq > 0) {
                    double k = std::sqrt(ksq);
                    // Evaluate Deltak = k^3/(2pi^2) P(k)
                    double Deltak = getPower(k);
                    // Calculate corresponding RMS for Re,Im parts of delta_k
                    sigma = std::sqrt(Deltak*dk3/(ksq*k)/2);
                    //sigma = std::exp(-(k-.2)*(k-.2)/.00001);
                    //sigma = 1/(ksq);
                }
                std::size_t index(iz+_halfz*(iy+getNy()*ix));
                //std::cout << index << " " << sigma << std::endl;
                _pimpl->data[index][0] *= sigma;//sigma;
                _pimpl->data[index][1] *= sigma;//sigma;
            }
        }
    }
#endif    
}

void local::FftGaussianRandomFieldGenerator::transformFieldToR() {
#ifdef HAVE_LIBFFTW3F
    // Do the inverse FFT.
    FFTW(execute)(_pimpl->plan);
#endif
}

void local::FftGaussianRandomFieldGenerator::generate() {
#ifdef HAVE_LIBFFTW3F
    generateFieldK();
    transformFieldToR();
#endif
}

int local::FftGaussianRandomFieldGenerator::flattenIndex(int kx, int ky, int kz) const{
    return kz+_halfz*(ky+getNy()*kx);
}

double local::FftGaussianRandomFieldGenerator::getFieldKRe(int kx, int ky, int kz) const {
    if(kx < 0 || kx >= getNx()) {
        throw RuntimeError("FftGaussianRandomFieldGenerator: invalid kx < 0 or >= nx.");
    }
    if(ky < 0 || ky >= getNy()) {
        throw RuntimeError("FftGaussianRandomFieldGenerator: invalid ky < 0 or >= ny.");
    }
    if(kz < 0 || kz >= getNz()) {
        throw RuntimeError("FftGaussianRandomFieldGenerator: invalid kz < 0 or >= nz.");
    }
    if(2*kz == getNz() && ky == 0 && kx == 0){
        return _pimpl->data[flattenIndex(kx,ky,kz)][0];
    }
    if( kz == 0 || 2*kz == getNz() ){
        if(ky != 0 && kx == 0){
            return .5*(_pimpl->data[flattenIndex(kx,ky,kz)][0]+_pimpl->data[flattenIndex(kx,getNy()-ky,kz)][0]);
        }
        else if( ky == 0 && kx != 0) {
            return .5*(_pimpl->data[flattenIndex(kx,ky,kz)][0]+_pimpl->data[flattenIndex(getNx()-kx,ky,kz)][0]);
        }
        else if( kx != 0 && ky != 0) {
            return .5*(_pimpl->data[flattenIndex(kx,ky,kz)][0]+_pimpl->data[flattenIndex(getNx()-kx,getNy()-ky,kz)][0]);            
        }
    }
    if(kz < _halfz){
        return _pimpl->data[flattenIndex(kx,ky,kz)][0];
    }
    else { 
        if(ky != 0) {
            ky = getNy()-ky;
        }
        if(kx != 0) {
            kx = getNx()-kx;
        }
        return _pimpl->data[flattenIndex(kx,ky,getNz()-kz)][0];
    }
}

double local::FftGaussianRandomFieldGenerator::getFieldKIm(int kx, int ky, int kz) const {
    if(kx < 0 || kx >= getNx()) {
        throw RuntimeError("FftGaussianRandomFieldGenerator: invalid kx < 0 or >= nx.");
    }
    if(ky < 0 || ky >= getNy()) {
        throw RuntimeError("FftGaussianRandomFieldGenerator: invalid ky < 0 or >= ny.");
    }
    if(kz < 0 || kz >= getNz()) {
        throw RuntimeError("FftGaussianRandomFieldGenerator: invalid kz < 0 or >= nz.");
    }
    if(2*kz == getNz() && ky == 0 && kx == 0){
        return 0;
    }
    if( kz == 0 || 2*kz == getNz() ){
        if(ky != 0 && kx == 0){
            return .5*(_pimpl->data[flattenIndex(kx,ky,kz)][1]-_pimpl->data[flattenIndex(kx,getNy()-ky,kz)][1]);
        }
        else if( ky == 0 && kx != 0) {
            return .5*(_pimpl->data[flattenIndex(kx,ky,kz)][1]-_pimpl->data[flattenIndex(getNx()-kx,ky,kz)][1]);
        }
        else if( kx != 0 && ky != 0) {
            return .5*(_pimpl->data[flattenIndex(kx,ky,kz)][1]-_pimpl->data[flattenIndex(getNx()-kx,getNy()-ky,kz)][1]);            
        }
    }
    if(kz < _halfz){
        return _pimpl->data[flattenIndex(kx,ky,kz)][1];
    }
    else {
        if(ky != 0) {
            ky = getNy()-ky;
        }
        if(kx != 0) {
            kx = getNx()-kx;
        }
        return -_pimpl->data[flattenIndex(kx,ky,getNz()-kz)][1];
    }
}


double local::FftGaussianRandomFieldGenerator::_getFieldUnchecked(int x, int y, int z) const {
    FftwReal const *realData = (FftwReal*)(_pimpl->data);
    int index(z+2*_halfz*(y+getNy()*x));
    return (double)realData[index];
}

std::size_t local::FftGaussianRandomFieldGenerator::getMemorySize() const {
    return sizeof(*this) + (std::size_t)getNx()*getNy()*_halfz*8;
}
