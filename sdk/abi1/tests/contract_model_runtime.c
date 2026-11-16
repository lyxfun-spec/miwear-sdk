/* Test-only freestanding result sink, never part of an SDK archive. */
unsigned contract_cases, contract_checks, contract_failure;
int printf(const char *format, unsigned cases, unsigned checks, unsigned failure) {
    (void)format;
    contract_cases = cases;
    contract_checks = checks;
    contract_failure = failure;
    return 0;
}
int memcmp(const void *left, const void *right, unsigned count) {
    const unsigned char *a = left, *b = right;
    for (unsigned i = 0; i < count; ++i)
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    return 0;
}
