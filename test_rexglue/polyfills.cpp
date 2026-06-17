// Polyfills for C23 math functions not available in MSVC CRT
#include <cmath>

extern "C" {

// roundevenf: round to nearest, ties to even (C23)
float roundevenf(float x) {
    float int_part;
    float frac = std::modff(x, &int_part);
    int int_val = (int)std::fabs(int_part);

    if (std::fabs(frac) < 0.5f) {
        return int_part;
    } else if (std::fabs(frac) > 0.5f) {
        return int_part + (x > 0 ? 1.0f : -1.0f);
    } else {
        // Exactly 0.5 — round to even
        if (int_val % 2 == 0) {
            return int_part;
        } else {
            return int_part + (x > 0 ? 1.0f : -1.0f);
        }
    }
}

double roundeven(double x) {
    double int_part;
    double frac = std::modf(x, &int_part);
    long long int_val = (long long)std::fabs(int_part);

    if (std::fabs(frac) < 0.5) {
        return int_part;
    } else if (std::fabs(frac) > 0.5) {
        return int_part + (x > 0 ? 1.0 : -1.0);
    } else {
        // Exactly 0.5 — round to even
        if (int_val % 2 == 0) {
            return int_part;
        } else {
            return int_part + (x > 0 ? 1.0 : -1.0);
        }
    }
}

} // extern "C"
