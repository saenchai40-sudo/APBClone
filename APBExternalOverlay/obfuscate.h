#ifndef OBFUSCATE_H
#define OBFUSCATE_H

#include <string>


namespace Obfuscation {
    template<int N>
    struct XorString {
        char value[N];

        constexpr XorString(const char(&s)[N]) : value() {
            for (int i = 0; i < N; ++i) {
                value[i] = s[i] ^ (char)(N + i);
            }
        }

        const char* decrypt() const {
            static char decrypted[N];
            for (int i = 0; i < N; ++i) {
                decrypted[i] = value[i] ^ (char)(N + i);
            }
            return decrypted;
        }
    };

    template<int N>
    struct XorWString {
        wchar_t value[N];

        constexpr XorWString(const wchar_t(&s)[N]) : value() {
            for (int i = 0; i < N; ++i) {
                value[i] = s[i] ^ (wchar_t)(N + i);
            }
        }

        const wchar_t* decrypt() const {
            static wchar_t decrypted[N];
            for (int i = 0; i < N; ++i) {
                decrypted[i] = value[i] ^ (wchar_t)(N + i);
            }
            return decrypted;
        }
    };
}

#define OBFUSCATE(s) (Obfuscation::XorString<sizeof(s)>(s)).decrypt()
#define OBFUSCATE_W(s) (Obfuscation::XorWString<sizeof(s)/sizeof(wchar_t)>(s)).decrypt()

#endif // OBFUSCATE_H
