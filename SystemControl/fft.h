#ifndef FFT_H
#define FFT_H


/* COMPLEX STRUCTURE */

typedef struct {
	 float real, imag;
} COMPLEX;


void fft(COMPLEX *x, COMPLEX *w, unsigned int m);


#endif // !SYSTEMCONTROL_H
