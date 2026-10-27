// SPDX-License-Identifier: Apache-2.0
#include "openssl_backend.hpp"
#include <cassert>
#include <array>
int main(){
 OpenSSLCrypto crypto;std::array<unsigned char,32> key{},aad{},plaintext{};std::array<unsigned char,12> nonce{};std::array<unsigned char,16> sealed{};
 assert(crypto.Aes256GcmSeal({key.data(),32},{nonce.data(),12},{aad.data(),32},{},{sealed.data(),16}).ok());
 assert(crypto.Aes256GcmOpen({key.data(),32},{nonce.data(),12},{aad.data(),32},{sealed.data(),16},{plaintext.data(),32}).ok());
 sealed[0]^=1;assert(!crypto.Aes256GcmOpen({key.data(),32},{nonce.data(),12},{aad.data(),32},{sealed.data(),16},{plaintext.data(),32}).ok());
 std::array<unsigned char,32> first{},second{};nc::ByteSpan outputs[]={{first.data(),32},{second.data(),32}};
 assert(crypto.HkdfSha256({key.data(),32},{},{},{outputs,2}).ok());
 // RFC 5869 with salt=32 zero bytes, empty IKM and empty info.
 const unsigned char expected[32]={0xeb,0x70,0xf0,0x1d,0xed,0xe9,0xaf,0xaf,0xa4,0x49,0xee,0xe1,0xb1,0x28,0x65,0x04,0xe1,0xf6,0x23,0x88,0xb3,0xf7,0xdd,0x4f,0x95,0x66,0x97,0xb0,0xe8,0x28,0xfe,0x18};
 assert(memcmp(first.data(),expected,32)==0);
}
