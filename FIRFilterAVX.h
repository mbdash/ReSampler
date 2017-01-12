#ifndef FIRFFILTER_AVX_H_
#define FIRFFILTER_AVX_H_
#ifdef USE_AVX

#define AVX_ALIGNMENT_SIZE 32

//#define USE_FMA 1

// FIRFilterAVX.h : AVX-specific code for FIR filtering

#include <immintrin.h>
#include <typeinfo>
#include <cstdint>
#include <cassert>

#include "alignedmalloc.h"

template <typename FloatType>
class FIRFilter {

public:
	// constructor:
	FIRFilter(const FloatType* taps, size_t size) :
		size(size), CurrentIndex(size-1), LastPut(0),
		Signal(NULL), Kernel0(NULL), Kernel1(NULL), Kernel2(NULL), Kernel3(NULL), Kernel4(NULL), Kernel5(NULL), Kernel6(NULL), Kernel7(NULL)
	{
		// allocate buffers:
		allocateBuffers();
		assertAlignment();

		// initialize filter kernel and signal buffers
		for (unsigned int i = 0; i < size; ++i) {
			Kernel0[i] = taps[i];
			Signal[i] = 0.0;
			Signal[i + size] = 0.0;
		}

		// Populate additional kernel Phases:
		memcpy(1 + Kernel1, Kernel0, (size - 1) * sizeof(FloatType));
		Kernel1[0] = Kernel0[size - 1];
		memcpy(1 + Kernel2, Kernel1, (size - 1) * sizeof(FloatType));
		Kernel2[0] = Kernel1[size - 1];
		memcpy(1 + Kernel3, Kernel2, (size - 1) * sizeof(FloatType));
		Kernel3[0] = Kernel2[size - 1];
		//
		memcpy(1 + Kernel4, Kernel3, (size - 1) * sizeof(FloatType));
		Kernel4[0] = Kernel3[size - 1];
		memcpy(1 + Kernel5, Kernel4, (size - 1) * sizeof(FloatType));
		Kernel5[0] = Kernel4[size - 1];
		memcpy(1 + Kernel6, Kernel5, (size - 1) * sizeof(FloatType));
		Kernel6[0] = Kernel5[size - 1];
		memcpy(1 + Kernel7, Kernel6, (size - 1) * sizeof(FloatType));
		Kernel7[0] = Kernel6[size - 1];

	}

	// deconstructor:
	~FIRFilter() {
		freeBuffers();
	}

	// copy constructor: 
	FIRFilter(const FIRFilter& other) : size(other.size), CurrentIndex(other.CurrentIndex), LastPut(other.LastPut)
	{
		allocateBuffers();
		assertAlignment();
		copyBuffers(other);
	}

	// move constructor:
	FIRFilter(FIRFilter&& other) :
		size(other.size), CurrentIndex(other.CurrentIndex), LastPut(other.LastPut),
		Signal(other.Signal), Kernel0(other.Kernel0), Kernel1(other.Kernel1), Kernel2(other.Kernel2), Kernel3(other.Kernel3),
		Kernel4(other.Kernel4), Kernel5(other.Kernel5), Kernel6(other.Kernel6), Kernel7(other.Kernel7)
	{
		assertAlignment();
		other.Signal = nullptr;
		other.Kernel0 = nullptr;
		other.Kernel1 = nullptr;
		other.Kernel2 = nullptr;
		other.Kernel3 = nullptr;
		other.Kernel4 = nullptr;
		other.Kernel5 = nullptr;
		other.Kernel6 = nullptr;
		other.Kernel7 = nullptr;
	}

	// copy assignment:
	FIRFilter& operator= (const FIRFilter& other)
	{
		size = other.size;
		CurrentIndex = other.CurrentIndex;
		LastPut = other.LastPut;
		freeBuffers();
		allocateBuffers();
		assertAlignment();
		copyBuffers(other);
		return *this;
	}

	// move assignment:
	FIRFilter& operator= (FIRFilter&& other)
	{
		if (this != &other) // prevent self-assignment
		{
			size = other.size;
			CurrentIndex = other.CurrentIndex;
			LastPut = other.LastPut;

			freeBuffers();

			Signal = other.Signal;
			Kernel0 = other.Kernel0;
			Kernel1 = other.Kernel1;
			Kernel2 = other.Kernel2;
			Kernel3 = other.Kernel3;
			Kernel4 = other.Kernel4;
			Kernel5 = other.Kernel5;
			Kernel6 = other.Kernel6;
			Kernel7 = other.Kernel7;

			assertAlignment();

			other.Signal = nullptr;
			other.Kernel0 = nullptr;
			other.Kernel1 = nullptr;
			other.Kernel2 = nullptr;
			other.Kernel3 = nullptr;
			other.Kernel4 = nullptr;
			other.Kernel5 = nullptr;
			other.Kernel6 = nullptr;
			other.Kernel7 = nullptr;
		}
		return *this;
	}
	
	void put(FloatType value) { // Put signal in reverse order.
		Signal[CurrentIndex] = value;
		LastPut = CurrentIndex;
		if (CurrentIndex == 0) {
			CurrentIndex = size - 1; // Wrap
			memcpy(Signal + size, Signal, size*sizeof(FloatType)); // copy history to upper half of buffer
		}
		else
			--CurrentIndex;
	}

	void putZero() {
		Signal[CurrentIndex] = 0.0;
		if (CurrentIndex == 0) {
			CurrentIndex = size - 1; // Wrap
			memcpy(Signal + size, Signal, size*sizeof(FloatType)); // copy history to upper half of buffer
		}
		else
			--CurrentIndex;
	}

	FloatType get() {

		// AVX implementation: This only works with floats - doubles need specialization ...

		FloatType output = 0.0;
		FloatType* Kernel = Kernel0;
		int Index = (CurrentIndex >> 3) << 3; // make multiple-of-eight
		int Phase = CurrentIndex & 7;
		
		// Part1 : Head
		// select proper Kernel phase and calculate first Block of 8:
		switch (Phase) {

		case 0:
			Kernel = Kernel0;
			output =
				Kernel[0] * Signal[Index] + Kernel[1] * Signal[Index + 1] + Kernel[2] * Signal[Index + 2] + Kernel[3] * Signal[Index + 3] +
				Kernel[4] * Signal[Index + 4] + Kernel[5] * Signal[Index + 5] + Kernel[6] * Signal[Index + 6] + Kernel[7] * Signal[Index + 7];
			break;

		case 1:
			Kernel = Kernel1;
			output =
				Kernel[0] * Signal[Index + size] + Kernel[1] * Signal[Index + 1] + Kernel[2] * Signal[Index + 2] + Kernel[3] * Signal[Index + 3] +
				Kernel[4] * Signal[Index + 4] + Kernel[5] * Signal[Index + 5] + Kernel[6] * Signal[Index + 6] + Kernel[7] * Signal[Index + 7];
			break;

		case 2:
			Kernel = Kernel2;
			output =
				Kernel[0] * Signal[Index + size] + Kernel[1] * Signal[Index + size + 1] + Kernel[2] * Signal[Index + 2] + Kernel[3] * Signal[Index + 3] +
				Kernel[4] * Signal[Index + 4] + Kernel[5] * Signal[Index + 5] + Kernel[6] * Signal[Index + 6] + Kernel[7] * Signal[Index + 7];
			break;

		case 3: 
			Kernel = Kernel3;
			output =
				Kernel[0] * Signal[Index + size] + Kernel[1] * Signal[Index + size + 1] + Kernel[2] * Signal[Index + size + 2] + Kernel[3] * Signal[Index + 3] +
				Kernel[4] * Signal[Index + 4] + Kernel[5] * Signal[Index + 5] + Kernel[6] * Signal[Index + 6] + Kernel[7] * Signal[Index + 7];
			break;

		case 4:
			Kernel = Kernel4;
			output =
				Kernel[0] * Signal[Index + size] + Kernel[1] * Signal[Index + size + 1] + Kernel[2] * Signal[Index + size + 2] + Kernel[3] * Signal[Index + size + 3] +
				Kernel[4] * Signal[Index + 4] + Kernel[5] * Signal[Index + 5] + Kernel[6] * Signal[Index + 6] + Kernel[7] * Signal[Index + 7];
			break;

		case 5:
			Kernel = Kernel5;
			output =
				Kernel[0] * Signal[Index + size] + Kernel[1] * Signal[Index + size + 1] + Kernel[2] * Signal[Index + size + 2] + Kernel[3] * Signal[Index + size + 3] +
				Kernel[4] * Signal[Index + size + 4] + Kernel[5] * Signal[Index + 5] + Kernel[6] * Signal[Index + 6] + Kernel[7] * Signal[Index + 7];
			break;

		case 6:
			Kernel = Kernel6;
			output =
				Kernel[0] * Signal[Index + size] + Kernel[1] * Signal[Index + size + 1] + Kernel[2] * Signal[Index + size + 2] + Kernel[3] * Signal[Index + size + 3] +
				Kernel[4] * Signal[Index + size + 4] + Kernel[5] * Signal[Index + size + 5] + Kernel[6] * Signal[Index + 6] + Kernel[7] * Signal[Index + 7];
			break;

		case 7:
			Kernel = Kernel7;
			output =
				Kernel[0] * Signal[Index + size] + Kernel[1] * Signal[Index + size + 1] + Kernel[2] * Signal[Index + size + 2] + Kernel[3] * Signal[Index + size + 3] +
				Kernel[4] * Signal[Index + size + 4] + Kernel[5] * Signal[Index + size + 5] + Kernel[6] * Signal[Index + size + 6] + Kernel[7] * Signal[Index + 7];
			break;
		}
		Index += 8;

		// Part 2: Body
		alignas(AVX_ALIGNMENT_SIZE) __m256 signal;	// AVX Vector Registers for calculation
		alignas(AVX_ALIGNMENT_SIZE) __m256 kernel;
		alignas(AVX_ALIGNMENT_SIZE) __m256 product;
		alignas(AVX_ALIGNMENT_SIZE) __m256 accumulator = _mm256_setzero_ps();

		for (int i = 8; i < (size >> 3) << 3; i += 8) {
			signal = _mm256_load_ps(Signal + Index);
			kernel = _mm256_load_ps(Kernel + i);
#ifdef USE_FMA
			accumulator = _mm256_fmadd_ps(signal, kernel, accumulator);
#else
			product = _mm256_mul_ps(signal, kernel);
			accumulator = _mm256_add_ps(product, accumulator);
#endif

			Index += 8;
		}	

		output += 
			accumulator.m256_f32[0] +
			accumulator.m256_f32[1] +
			accumulator.m256_f32[2] +
			accumulator.m256_f32[3] +
			accumulator.m256_f32[4] +
			accumulator.m256_f32[5] +
			accumulator.m256_f32[6] +
			accumulator.m256_f32[7];

		// Part 3: Tail
		for (int j = (size >> 3) << 3; j < size; ++j) {
			output += Signal[Index] * Kernel[j];
			++Index;
		}

		return output;
	}

	FloatType LazyGet(int L) {	// Skips stuffed-zeros introduced by interpolation, by only calculating every Lth sample from LastPut
		FloatType output = 0.0;
		int Offset = LastPut - CurrentIndex;
		if (Offset < 0) { // Wrap condition
			Offset += size;
		}
	
		for (int i = Offset; i < size; i+=L) {
			output += Signal[i+ CurrentIndex] * Kernel0[i];
		}
		return output;
	}

private:
	size_t size;
	FloatType* Signal; // Double-length signal buffer, to facilitate fast emulation of a circular buffer
	int CurrentIndex;
	int LastPut;

	// Polyphase Filter Kernel table:
	FloatType* Kernel0;
	FloatType* Kernel1;
	FloatType* Kernel2;
	FloatType* Kernel3;
	FloatType* Kernel4;
	FloatType* Kernel5;
	FloatType* Kernel6;
	FloatType* Kernel7;

	void allocateBuffers()
	{
		Signal = static_cast<FloatType*>(aligned_malloc(2 * size * sizeof(FloatType), AVX_ALIGNMENT_SIZE));
		Kernel0 = static_cast<FloatType*>(aligned_malloc(size * sizeof(FloatType), AVX_ALIGNMENT_SIZE));
		Kernel1 = static_cast<FloatType*>(aligned_malloc(size * sizeof(FloatType), AVX_ALIGNMENT_SIZE));
		Kernel2 = static_cast<FloatType*>(aligned_malloc(size * sizeof(FloatType), AVX_ALIGNMENT_SIZE));
		Kernel3 = static_cast<FloatType*>(aligned_malloc(size * sizeof(FloatType), AVX_ALIGNMENT_SIZE));
		Kernel4 = static_cast<FloatType*>(aligned_malloc(size * sizeof(FloatType), AVX_ALIGNMENT_SIZE));
		Kernel5 = static_cast<FloatType*>(aligned_malloc(size * sizeof(FloatType), AVX_ALIGNMENT_SIZE));
		Kernel6 = static_cast<FloatType*>(aligned_malloc(size * sizeof(FloatType), AVX_ALIGNMENT_SIZE));
		Kernel7 = static_cast<FloatType*>(aligned_malloc(size * sizeof(FloatType), AVX_ALIGNMENT_SIZE));
	}

	void copyBuffers(const FIRFilter& other)
	{
		memcpy(Signal, other.Signal, 2 * size * sizeof(FloatType));
		memcpy(Kernel0, other.Kernel0, size * sizeof(FloatType));
		memcpy(Kernel1, other.Kernel1, size * sizeof(FloatType));
		memcpy(Kernel2, other.Kernel2, size * sizeof(FloatType));
		memcpy(Kernel3, other.Kernel3, size * sizeof(FloatType));
		memcpy(Kernel4, other.Kernel4, size * sizeof(FloatType));
		memcpy(Kernel5, other.Kernel5, size * sizeof(FloatType));
		memcpy(Kernel6, other.Kernel6, size * sizeof(FloatType));
		memcpy(Kernel7, other.Kernel7, size * sizeof(FloatType));
	}

	void freeBuffers()
	{
		aligned_free(Signal);
		aligned_free(Kernel0);
		aligned_free(Kernel1);
		aligned_free(Kernel2);
		aligned_free(Kernel3);
		aligned_free(Kernel4);
		aligned_free(Kernel5);
		aligned_free(Kernel6);
		aligned_free(Kernel7);
	}

	// assertAlignment() : asserts that all private data buffers are aligned on expected boundaries
	void assertAlignment()
	{
		const std::uintptr_t alignment = AVX_ALIGNMENT_SIZE;
		
		assert(reinterpret_cast<std::uintptr_t>(Signal) % alignment == 0);
		assert(reinterpret_cast<std::uintptr_t>(Kernel0) % alignment == 0);
		assert(reinterpret_cast<std::uintptr_t>(Kernel1) % alignment == 0);
		assert(reinterpret_cast<std::uintptr_t>(Kernel2) % alignment == 0);
		assert(reinterpret_cast<std::uintptr_t>(Kernel3) % alignment == 0);
		assert(reinterpret_cast<std::uintptr_t>(Kernel4) % alignment == 0);
		assert(reinterpret_cast<std::uintptr_t>(Kernel5) % alignment == 0);
		assert(reinterpret_cast<std::uintptr_t>(Kernel6) % alignment == 0);
		assert(reinterpret_cast<std::uintptr_t>(Kernel7) % alignment == 0);

		/*
		// for diagnostics (report to stdout) ...
		if (
			(reinterpret_cast<std::uintptr_t>(Signal) % alignment == 0) &&
			(reinterpret_cast<std::uintptr_t>(Kernel0) % alignment == 0) &&
			(reinterpret_cast<std::uintptr_t>(Kernel1) % alignment == 0) &&
			(reinterpret_cast<std::uintptr_t>(Kernel2) % alignment == 0) &&
			(reinterpret_cast<std::uintptr_t>(Kernel3) % alignment == 0) &&
			(reinterpret_cast<std::uintptr_t>(Kernel4) % alignment == 0) &&
			(reinterpret_cast<std::uintptr_t>(Kernel5) % alignment == 0) &&
			(reinterpret_cast<std::uintptr_t>(Kernel6) % alignment == 0) &&
			(reinterpret_cast<std::uintptr_t>(Kernel7) % alignment == 0)
			) {
			std::cout << "Data Alignment OK !" << std::endl;
		}
		else {
			std::cout << "Data Alignment BAD !" << std::endl;
		}
		*/

	}
};

// ================================= 
// AVX specializations for doubles :
// =================================

double FIRFilter<double>::get() {

	// AVX implementation: This only works with doubles !
	// Processes four doubles at a time.

	double output = 0.0;
	double* Kernel;
	int Index = (CurrentIndex >> 2) << 2; // make multiple-of-four
	int Phase = CurrentIndex & 3;

	// Part1 : Head
	// select proper Kernel phase and calculate first Block of 4:
	switch (Phase) {

	case 0:
		Kernel = Kernel0;
		output = Kernel[0] * Signal[Index] + Kernel[1] * Signal[Index + 1] + Kernel[2] * Signal[Index + 2] + Kernel[3] * Signal[Index + 3];
		break;

	case 1:
		Kernel = Kernel1;
		output = Kernel[0] * Signal[Index + size] + Kernel[1] * Signal[Index + 1] + Kernel[2] * Signal[Index + 2] + Kernel[3] * Signal[Index + 3];
		break;

	case 2:
		Kernel = Kernel2;
		output = Kernel[0] * Signal[Index + size] + Kernel[1] * Signal[Index + size + 1] + Kernel[2] * Signal[Index + 2] + Kernel[3] * Signal[Index + 3];
		break;

	case 3:
		Kernel = Kernel3;
		output = Kernel[0] * Signal[Index + size] + Kernel[1] * Signal[Index + size + 1] + Kernel[2] * Signal[Index + size + 2] + Kernel[3] * Signal[Index + 3];
		break;

	}
	Index += 4;

	// Part 2: Body
	alignas(AVX_ALIGNMENT_SIZE) __m256d signal;	// AVX Vector Registers for calculation
	alignas(AVX_ALIGNMENT_SIZE) __m256d kernel;
	alignas(AVX_ALIGNMENT_SIZE) __m256d product;
	alignas(AVX_ALIGNMENT_SIZE) __m256d accumulator = _mm256_setzero_pd();

	for (int i = 4; i < (size >> 2) << 2; i += 4) {
		signal = _mm256_load_pd(Signal + Index);
		kernel = _mm256_load_pd(Kernel + i);
		product = _mm256_mul_pd(signal, kernel);
		accumulator = _mm256_add_pd(product, accumulator);
		Index += 4;
	}

	output +=
		accumulator.m256d_f64[0] +
		accumulator.m256d_f64[1] +
		accumulator.m256d_f64[2] +
		accumulator.m256d_f64[3];

	// Part 3: Tail
	for (int j = (size >> 2) << 2; j < size; ++j) {
		output += Signal[Index] * Kernel[j];
		++Index;
	}

	return output;
}
#endif // USE_AVX
#endif // FIRFFILTER_AVX_H_