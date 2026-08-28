#include <cstring>
#include <cstdio>
#include <cstdlib>

typedef unsigned long long Decimal;

static double to_d(Decimal x) {
    double d;
    std::memcpy(&d, &x, 8);
    return d;
}

static Decimal from_d(double d) {
    Decimal x;
    std::memcpy(&x, &d, 8);
    return x;
}

extern "C" {

Decimal __bid64_add(Decimal a, Decimal b, unsigned int, unsigned int* f) {
    if (f) *f = 0;
    return from_d(to_d(a) + to_d(b));
}
Decimal __bid64_sub(Decimal a, Decimal b, unsigned int, unsigned int* f) {
    if (f) *f = 0;
    return from_d(to_d(a) - to_d(b));
}
Decimal __bid64_mul(Decimal a, Decimal b, unsigned int, unsigned int* f) {
    if (f) *f = 0;
    return from_d(to_d(a) * to_d(b));
}
Decimal __bid64_div(Decimal a, Decimal b, unsigned int, unsigned int* f) {
    if (f) *f = 0;
    return from_d(to_d(a) / to_d(b));
}
Decimal __bid64_from_string(char* s, unsigned int, unsigned int* f) {
    if (f) *f = 0;
    return from_d(std::strtod(s, nullptr));
}
void __bid64_to_string(char* out, Decimal x, unsigned int* f) {
    if (f) *f = 0;
    std::sprintf(out, "%.16g", to_d(x));
}
double __bid64_to_binary64(Decimal x, unsigned int, unsigned int* f) {
    if (f) *f = 0;
    return to_d(x);
}
Decimal __binary64_to_bid64(double d, unsigned int, unsigned int* f) {
    if (f) *f = 0;
    return from_d(d);
}

}