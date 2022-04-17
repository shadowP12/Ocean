/*

This file is am_fft.h - a single-header library for limited fast fourier transforms.
Copyright (c) 2018 Andreas Mantler
Distributed under the MIT License:

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#ifndef AM_FFT_H
#define AM_FFT_H


// NOTE: This library is currently limited to FFTs with power-of-two sizes and, in case of 2D dfts, a square shape.


// The complex type { real, imaginary }:
typedef float am_fft_complex_t[2];

// Plans (hold precomputed coefficients to quickly re-process data with the same layout):
typedef struct am_fft_plan_1d_ am_fft_plan_1d_t;
typedef struct am_fft_plan_2d_ am_fft_plan_2d_t;

// FFT directions:
#define AM_FFT_FORWARD 0
#define AM_FFT_INVERSE 1

// Functions:
am_fft_plan_1d_t* am_fft_plan_1d(int direction, unsigned int n);
void              am_fft_plan_1d_free(am_fft_plan_1d_t *plan);
void              am_fft_1d(const am_fft_plan_1d_t *plan, const am_fft_complex_t *in, am_fft_complex_t *out);

am_fft_plan_2d_t* am_fft_plan_2d(int direction, unsigned int width, unsigned int height);
void              am_fft_plan_2d_free(am_fft_plan_2d_t *plan);
void              am_fft_2d(const am_fft_plan_2d_t *plan, const am_fft_complex_t *in, am_fft_complex_t *out);

#endif