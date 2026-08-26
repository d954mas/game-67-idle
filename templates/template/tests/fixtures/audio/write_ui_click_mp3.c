#include <stdint.h>
#include <stdio.h>

static const char s_fixture_base64[] =
    "//tAwAAAAAAAAAAAAAAAAAAAAAAAWGluZwAAAA8AAAAFAAAHbwB6enp6enp6enp6enp6enp6enp6srKysrKysrKysrKysrKy"
    "srKysrLOzs7Ozs7Ozs7Ozs7Ozs7Ozs7OzvLy8vLy8vLy8vLy8vLy8vLy8vLy//////////////////////////8AAAAATGF2"
    "YzYyLjIzAAAAAAAAAAAAAAAAJASWAAAAAAAAB2/uiP7TAAAAAAD/+8DEAAAFrANHlAAAI5yyan83gAAgpUBoyonAnD5QMWCA"
    "MQQdD4IOy4PvnJcHwfBx3xOD/wfB8P/g+//6AQd+DgIMaZDzeO8M4qw6m4bX5/OOGeCC8DEQ1q4YsCQeZiekSEIRIIMguGCN"
    "JIgYKhZRDkRMNnxmwMsZqMgZkz0wKcRPGSOixBaVLLkbGCO42KpJX2Z+6iiCPBcRtHliObexjj2O4wN+Je28uR/huff+DZ7W"
    "HKsqhDuS2Lros07uTjvyamlFJnci0I3MSPVMuiV0lMzhTR1KO3Ul83nWr6kD7v/OQxZ1lqLO44EFNcilJjL797HLO9dm6emw"
    "xu6paGWVPtZ9x5apqk4/mPXfjfO81rHW9f3+f/f7hn///6//+Xzedp/JZzteel/VMWqKm4hGIFJCDRRkSvbssEyIcPEhTNTw"
    "FwNdKQRgQA9GQXM6zHEpmQbQBEMJhwYFAgMOuZnP4qMusgUyxkCXU67sBGcUudEAJEImCvIz9cMTjjOm5X5qH3ZUefRQeljz"
    "lz8OWGtTOMsQGQS0G697XbVFF4cqW3Kl0fvX8C69JS4uVBdyV14xlSQ+/FNDL87lXx6pDDVX2m5BZ3egWNU+X6/9uTDkok3f"
    "/+wVPyp2LstiMgpJZnnSU93X/hXq2IZjUlvUtmah63qYnu2Zl+6KxK43HLmf9/PdeMSaFzktnu/uf79Nul5DU9lWjWX1X96L"
    "rUxDGJBMFNuDBHpAYBbhbh/AD5Rh0khENSSPMI/kG3MZRUIgWgeHkEhIWMZ3FWUoikOkFhIVKUcLsJtmczopSm0Q5Q7RDWy0"
    "ej2fUpbJlZd0KYqk//2Ixbdrt/zGmerWX/8tX9RyjhX1AVV428qnMyJJDjkA7D9FJLJRAL4/ROR2B1BGjKWjOS66b1n4JBsJ"
    "GE0SJy3OuYSxKTmLMWzZlpoKcqIGdC5UWj6G6shs1H2lQqOrozzIchBQpu3/BFZyl//7kMTYgB3ll235rBBBrK0qf55QAOVv"
    "+Uvkcrf/oaj+qwYm71E8tDshiKiKyCBKcjA0VYMQ6pEQE19k5IB4QRzAy66JLpePyrDFC6FhA1gwWOGMjZnMeSkd40d1okMK"
    "EDyQdYJWkRENvrHIETC44DpURJEUqIRyg01uKXs4K0HmD9k10r1WvSiGaZZCCphEJZroN5XCbs8asLJ18NNQ4uG/ywS7YHfx"
    "sQ0cBkzVFRW5kG1wzTFoxuAq2tOIWWZqJrbMVDCSLTgmgII6tA1SLngUClwiBQqVNFid5+IVnV66UH6nvSYWqQUYZ+091lUN"
    "EsyEVUUQPiZSYRsDVmbxgimDBEh2ipXg0BYLgaKDwpaRjdpzWaWxOQ7umgQwooEZtwywog/DQ4eKQrW6X2yqV8KxECPJ4jNG"
    "Y2pOhlSCMFWi5zNjFsXKpZeCbrWLKGjxm4CN/1pOW3Vkb/UAZoRkTUTUQ0XEXEyWW24KtVpfTRQ05WJ8+36rxmNtSDFGb2P/"
    "/jar+2vDqqpamq+xquq8ZmbjcY//oXNqUv1Cld+TYKNFf//7UMTxAAy1b0vnmE8BiI+m+YYNIGpMQU1FMy4xMDEgKGJldGEg"
    "Mymqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq"
    "qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq"
    "qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqr/+2DE6oAMvJ8zzBhuwZsbJnmEjWCqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq"
    "qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq"
    "qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq"
    "qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqr/"
    "+xDE/APJqPsrh4xRyAAANIAAAASqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq"
    "qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqg==";

static int base64_value(int ch) {
    if (ch >= 'A' && ch <= 'Z') return ch - 'A';
    if (ch >= 'a' && ch <= 'z') return ch - 'a' + 26;
    if (ch >= '0' && ch <= '9') return ch - '0' + 52;
    if (ch == '+') return 62;
    if (ch == '/') return 63;
    return -1;
}
int main(int argc, char **argv) {
    if (argc != 2) return 2;
    FILE *output = fopen(argv[1], "wb");
    if (output == NULL) return 3;

    uint32_t accumulator = 0U;
    unsigned bits = 0U;
    for (const char *cursor = s_fixture_base64; *cursor != '\0' && *cursor != '='; ++cursor) {
        const int value = base64_value((unsigned char)*cursor);
        if (value < 0) {
            fclose(output);
            return 4;
        }
        accumulator = (accumulator << 6U) | (uint32_t)value;
        bits += 6U;
        if (bits >= 8U) {
            bits -= 8U;
            if (fputc((int)((accumulator >> bits) & 0xffU), output) == EOF) {
                fclose(output);
                return 5;
            }
        }
    }
    return fclose(output) == 0 ? 0 : 6;
}
