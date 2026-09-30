/*********************************************************************************
 *  MIT License
 *  
 *  Copyright (c) 2020-2025 Gregg E. Berman
 *  
 *  https://github.com/HomeSpan/HomeSpan
 *  
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *  
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *  
 ********************************************************************************/
 
#pragma once

#include <mbedtls/sha512.h>
#include <mbedtls/bignum.h>
#include <mbedtls/base64.h>

#include "Utils.h"

/////////////////////////////////////////////////
// Pair-Setup Code Verification Data and Salt

struct Verification {
  uint8_t salt[16];
  uint8_t verifyCode[384];
};

/////////////////////////////////////////////////
// SRP-6A Structure from RFC 5054 (Nov 2007)
// ** HAP uses N=3072-bit Group specified in RFC 5054 with Generator g=5
// ** HAP replaces H=SHA-1 with H=SHA-512 (HAP Section 5.5)
//
// I = SRP-6A username, defined by HAP to be the word "Pair-Setup"
// P = SRP-6A password, defined to be equal to the accessory's 8-digit setup code in the format "XXX-XX-XXX"

struct SRP6A {

  static constexpr uint8_t N3072[384]={
                                  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xC9,0x0F,0xDA,0xA2,0x21,0x68,0xC2,0x34,0xC4,0xC6,0x62,0x8B,0x80,0xDC,0x1C,0xD1,
                                  0x29,0x02,0x4E,0x08,0x8A,0x67,0xCC,0x74,0x02,0x0B,0xBE,0xA6,0x3B,0x13,0x9B,0x22,0x51,0x4A,0x08,0x79,0x8E,0x34,0x04,0xDD,
                                  0xEF,0x95,0x19,0xB3,0xCD,0x3A,0x43,0x1B,0x30,0x2B,0x0A,0x6D,0xF2,0x5F,0x14,0x37,0x4F,0xE1,0x35,0x6D,0x6D,0x51,0xC2,0x45,
                                  0xE4,0x85,0xB5,0x76,0x62,0x5E,0x7E,0xC6,0xF4,0x4C,0x42,0xE9,0xA6,0x37,0xED,0x6B,0x0B,0xFF,0x5C,0xB6,0xF4,0x06,0xB7,0xED,
                                  0xEE,0x38,0x6B,0xFB,0x5A,0x89,0x9F,0xA5,0xAE,0x9F,0x24,0x11,0x7C,0x4B,0x1F,0xE6,0x49,0x28,0x66,0x51,0xEC,0xE4,0x5B,0x3D,
                                  0xC2,0x00,0x7C,0xB8,0xA1,0x63,0xBF,0x05,0x98,0xDA,0x48,0x36,0x1C,0x55,0xD3,0x9A,0x69,0x16,0x3F,0xA8,0xFD,0x24,0xCF,0x5F,
                                  0x83,0x65,0x5D,0x23,0xDC,0xA3,0xAD,0x96,0x1C,0x62,0xF3,0x56,0x20,0x85,0x52,0xBB,0x9E,0xD5,0x29,0x07,0x70,0x96,0x96,0x6D,
                                  0x67,0x0C,0x35,0x4E,0x4A,0xBC,0x98,0x04,0xF1,0x74,0x6C,0x08,0xCA,0x18,0x21,0x7C,0x32,0x90,0x5E,0x46,0x2E,0x36,0xCE,0x3B,
                                  0xE3,0x9E,0x77,0x2C,0x18,0x0E,0x86,0x03,0x9B,0x27,0x83,0xA2,0xEC,0x07,0xA2,0x8F,0xB5,0xC5,0x5D,0xF0,0x6F,0x4C,0x52,0xC9,
                                  0xDE,0x2B,0xCB,0xF6,0x95,0x58,0x17,0x18,0x39,0x95,0x49,0x7C,0xEA,0x95,0x6A,0xE5,0x15,0xD2,0x26,0x18,0x98,0xFA,0x05,0x10,
                                  0x15,0x72,0x8E,0x5A,0x8A,0xAA,0xC4,0x2D,0xAD,0x33,0x17,0x0D,0x04,0x50,0x7A,0x33,0xA8,0x55,0x21,0xAB,0xDF,0x1C,0xBA,0x64,
                                  0xEC,0xFB,0x85,0x04,0x58,0xDB,0xEF,0x0A,0x8A,0xEA,0x71,0x57,0x5D,0x06,0x0C,0x7D,0xB3,0x97,0x0F,0x85,0xA6,0xE1,0xE4,0xC7,
                                  0xAB,0xF5,0xAE,0x8C,0xDB,0x09,0x33,0xD7,0x1E,0x8C,0x94,0xE0,0x4A,0x25,0x61,0x9D,0xCE,0xE3,0xD2,0x26,0x1A,0xD2,0xEE,0x6B,
                                  0xF1,0x2F,0xFA,0x06,0xD9,0x8A,0x08,0x64,0xD8,0x76,0x02,0x73,0x3E,0xC8,0x6A,0x64,0x52,0x1F,0x2B,0x18,0x17,0x7B,0x20,0x0C,
                                  0xBB,0xE1,0x17,0x57,0x7A,0x61,0x5D,0x6C,0x77,0x09,0x88,0xC0,0xBA,0xD9,0x46,0xE2,0x08,0xE2,0x4F,0xA0,0x74,0xE5,0xAB,0x31,
                                  0x43,0xDB,0x5B,0xFC,0xE0,0xFD,0x10,0x8E,0x4B,0x82,0xD1,0x20,0xA9,0x3A,0xD2,0xCA,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

  static const uint8_t g3072=5;                   // generator

  static constexpr char I[]="Pair-Setup";

  mbedtls_mpi N;          // N                            - 3072-bit Group pre-defined prime used for all SRP-6A calculations (384 bytes)
  mbedtls_mpi g;          // g                            - pre-defined generator for the specified 3072-bit Group (g=5)
  mbedtls_mpi k;          // k = H(N | PAD(g))            - SRP-6A multiplier (which is different from versions SRP-6 or SRP-3) 
  mbedtls_mpi s;          // s                            - randomly-generated salt (16 bytes)
  mbedtls_mpi x;          // x = H(s | H(I | ":" | P))    - salted, double-hash of username and password (64 bytes)
  mbedtls_mpi v;          // v = g^x %N                   - SRP-6A verifier (max 384 bytes)  
  mbedtls_mpi b;          // b                            - randomly-generated private key for this HAP accessory (i.e. the SRP Server) (32 bytes)
  mbedtls_mpi B;          // B = k*v + g^b %N             - public key for this accessory (max 384 bytes)
  mbedtls_mpi A;          // A                            - public key RECEIVED from HAP Client (max 384 bytes)
  mbedtls_mpi u;          // u = H(PAD(A) | PAB(B))       - "u-factor" (64 bytes)
  mbedtls_mpi S;          // S = (A*v^u)^b %N             - SRP shared "premaster" key, based on accessory private key and client public key (max 384 bytes)
  uint8_t K[64];          // K = H(S)                     - SRP SHARED SECRET KEY (64 bytes)
  uint8_t M1[64];         // M1                           - proof RECEIVED from HAP Client (64 bytes)
  mbedtls_mpi t1;         // temp1                        - temporary mpi structures for intermediate results
  mbedtls_mpi t2;         // temp2                        - temporary mpi structures for intermediate results
  mbedtls_mpi t3;         // temp3                        - temporary mpi structures for intermediate results
  mbedtls_mpi _rr;        // _rr                          - temporary "helper" for large exponential modulus calculations


  SRP6A();                                         // initializes N, G, and computes k
  ~SRP6A();

  void *operator new(size_t size){return(HS_MALLOC(size));}     // override new operator to use PSRAM when available
  void operator delete(void *p){free(p);}
  
  void createVerifyCode(const char *setupCode, Verification *vData);                    // generates random s and computes v; writes back resulting Verification Data
  void createPublicKey(const Verification *vData, uint8_t *publicKey);                  // generates random b and computes k and B; writes back resulting Accessory Public Key 
  int createSessionKey(const uint8_t *publicKey, size_t len);                           // computes u, S, and K from Client Public Key, A (of variable length); returns 1 on success, 0 if A is invalid
  int verifyClientProof(const uint8_t *proof);                                          // verifies Client Proof, M1, received from HAP client (return 1 on success, 0 on failure)
  void createAccProof(uint8_t *proof);                                                  // computes M2; write back resulting Accessory Proof

  void print(mbedtls_mpi *mpi);   // prints size of mpi (in bytes), followed by the mpi itself (as a hex character string)
  
};
