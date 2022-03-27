#include <vector>
#include <iostream>
#include <ccomplex>

void DFT_1D(const std::vector<std::complex<float>>& in, std::vector<std::complex<float>>& out, int N) {
    for (int k = 0; k < N; ++k) {
        std::complex<float> X;
        for (int n = 0; n < N; ++n) {
            float real = X.real();
            float imag = X.imag();

            real += in[n].real() * cos(2 * 3.1415926 * n * k / N);
            imag -= in[n].real() * sin(2 * 3.1415926 * n * k / N);

            X.real(real);
            X.imag(imag);
        }
        out[k] = X;
    }
}

void DFT_2D(const std::vector<std::complex<float>>& in, std::vector<std::complex<float>>& out, int N, int M) {
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

int main() {
    // test 1d dft
    if (false)
    {
        std::vector<std::complex<float>> in(4);
        std::vector<std::complex<float>> out(4);

        in[0].real(8.0);
        in[1].real(4.0);
        in[2].real(8.0);
        in[3].real(0.0);

        DFT_1D(in, out, 4);

        for (int i = 0; i < 4; ++i) {
            printf("%d  %f  %f \n", i, out[i].real(), out[i].imag());
            printf("amplitude: %f\n", out[i].real());
            printf("phase: %f\n", out[i].imag());
        }
    }

    // test 2d dft
    if (true)
    {
    }

    return 0;
}
