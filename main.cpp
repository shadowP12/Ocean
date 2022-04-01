#include <vector>
#include <iostream>
#include <ccomplex>

#define PI 3.1415926

void DFT1D(std::vector<std::complex<float>>& in, std::vector<std::complex<float>>& out, int N) {
    for (int k = 0; k < N; ++k) {
        std::complex<float> X;
        for (int n = 0; n < N; ++n) {
            float real = X.real();
            float imag = X.imag();

            real += in[n].real() * cos(2 * PI * n * k / N);
            imag -= in[n].real() * sin(2 * PI * n * k / N);

            X.real(real);
            X.imag(imag);
        }
        out[k] = X;
    }
}

void DFT2D(std::vector<std::complex<float>>& in, std::vector<std::complex<float>>& out, int N, int M) {
    for (int u = 0; u < M; ++u) {
        for (int v = 0; v < N; ++v) {
            std::complex<float> F;
            for (int m = 0; m < M; ++m) {
                for (int n = 0; n < N; ++n) {
                    float real = F.real();
                    float imag = F.imag();

                    real += in[n * N + m].real() * cos(2 * 3.1415926 * ((u * m / M) + (v * n / N)));
                    imag -= in[n * N + m].real() * sin(2 * 3.1415926 * ((u * m / M) + (v * n / N)));

                    F.real(real);
                    F.imag(imag);
                }
            }
            out[v * N + u] = F;
        }
    }
}

struct LookUp {
    int j1, j2;
    float wr, wi;
};

int BitReverse(int i, int size) {
    int j = i;
    int Sum = 0;
    int W = 1;
    int M = size / 2;
    while (M != 0)
    {
        j = ((i & M) > M - 1) ? 1 : 0;
        Sum += j * W;
        W *= 2;
        M /= 2;
    }
    return Sum;
}

void FFT1D(std::complex<float>* in, std::complex<float>* out, int N) {
    int size = N;
    int passes = (int)(log(size) / log(2));
    LookUp* butterfly_lookup_table = new LookUp[size * passes];
    for (int i = 0; i < passes; i++) {
        int blocks = (int)pow(2, passes - 1 - i);
        int inputs = (int)pow(2, i);

        for (int j = 0; j < blocks; j++){
            for (int k = 0; k < inputs; k++) {
                int i1, i2, j1, j2;
                if (i == 0) {
                    i1 = j * inputs * 2 + k;
                    i2 = j * inputs * 2 + inputs + k;
                    j1 = BitReverse(i1, size);
                    j2 = BitReverse(i2, size);
                } else {
                    i1 = j * inputs * 2 + k;
                    i2 = j * inputs * 2 + inputs + k;
                    j1 = i1;
                    j2 = i2;
                }

                float wr = cos(2.0f * PI * (float)(k * blocks) / size);
                float wi = -sin(2.0f * PI * (float)(k * blocks) / size);

                int offset1 = (i1 + i * size);
                butterfly_lookup_table[offset1].j1 = j1;
                butterfly_lookup_table[offset1].j2 = j2;
                butterfly_lookup_table[offset1].wr = wr;
                butterfly_lookup_table[offset1].wi = wi;

                int offset2 = (i2 + i * size);
                butterfly_lookup_table[offset2].j1 = j1;
                butterfly_lookup_table[offset2].j2 = j2;
                butterfly_lookup_table[offset2].wr = -wr;
                butterfly_lookup_table[offset2].wi = -wi;
            }
        }
    }

    std::complex<float>* temp_data0 = new std::complex<float>[size];
    std::complex<float>* temp_data1 = new std::complex<float>[size];

    for (int i = 0; i < size; ++i) {
        temp_data0[i].real(in[i].real());
        temp_data0[i].imag(in[i].imag());
    }

    std::complex<float>* datas[] = {temp_data0, temp_data1};
    int read_idx = 0;
    int write_idx = 0;
    for (int i = 0; i < passes; i++) {
        read_idx = i % 2;
        write_idx = (i + 1) % 2;
        std::complex<float>* read = datas[read_idx];
        std::complex<float>* write = datas[write_idx];

        for (int j = 0; j < size; ++j) {
            int bft_idx = j + (i * size);
            int j1, j2;
            float wr, wi;
            j1 = butterfly_lookup_table[bft_idx].j1;
            j2 = butterfly_lookup_table[bft_idx].j2;
            wr = butterfly_lookup_table[bft_idx].wr;
            wi = butterfly_lookup_table[bft_idx].wi;

            write[j].real(read[j1].real() + wr * read[j2].real() - wi * read[j2].imag());
            write[j].imag(read[j1].imag() + wi * read[j2].real() + wr * read[j2].imag());
        }
    }

    for (int i = 0; i < size; ++i) {
        out[i].real(datas[write_idx][i].real());
        out[i].imag(datas[write_idx][i].imag());
    }

    delete [] temp_data0;
    delete [] temp_data1;
    delete [] butterfly_lookup_table;
}

int main() {
    // test 1d dft
    {
        printf("DFT1D : \n");
        std::vector<std::complex<float>> in(4);
        std::vector<std::complex<float>> out(4);

        in[0].real(8.0);
        in[1].real(4.0);
        in[2].real(8.0);
        in[3].real(0.0);

        DFT1D(in, out, 4);

        for (int i = 0; i < 4; ++i) {
            printf("%d  %f  %f \n", i, out[i].real(), out[i].imag());
            printf("amplitude: %f\n", out[i].real());
            printf("phase: %f\n", out[i].imag());
        }
    }

    // test 1d fft
    {
        printf("FFT1D : \n");

        std::complex<float>* in = new std::complex<float>[4];
        std::complex<float>* out = new std::complex<float>[4];

        in[0].real(-1.0);
        in[1].real(2.0);
        in[2].real(3.0);
        in[3].real(4.0);

        FFT1D(in, out, 4);

        for (int i = 0; i < 4; ++i) {
            printf("%d  %f  %f \n", i, out[i].real(), out[i].imag());
            printf("amplitude: %f\n", out[i].real());
            printf("phase: %f\n", out[i].imag());
        }

        delete [] in;
        delete [] out;
    }
    return 0;
}
