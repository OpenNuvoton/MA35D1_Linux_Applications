// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <cstring>
#include <xplat/noise/core/CryptoBackend.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

namespace nc = musegadgets::noise::core;
class OpenSSLCrypto final : public nc::CryptoBackend {
 using S=nc::Status; using B=nc::ByteSpan; using C=nc::ConstByteSpan;
 static S result(bool ok) noexcept {return ok?S::Ok():S::Internal();}
 public:
 S Random(B out) noexcept override {return result(RAND_bytes(out.data(),out.size())==1);}
 S X25519GenerateKeypair(B priv,B pub) noexcept override {
  if(priv.size()!=32||pub.size()!=32)return S::InvalidArgument();
  auto st=Random(priv);return st.ok()?X25519PublicFromPrivate(priv,pub):st;
 }
 S X25519PublicFromPrivate(C priv,B pub) noexcept override {
  if(priv.size()!=32||pub.size()!=32)return S::InvalidArgument();
  EVP_PKEY* key=EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519,nullptr,priv.data(),32);
  size_t len=32;bool ok=key&&EVP_PKEY_get_raw_public_key(key,pub.data(),&len)==1&&len==32;
  EVP_PKEY_free(key);return result(ok);
 }
 S X25519Dh(C priv,C peer,B out) noexcept override {
  if(priv.size()!=32||peer.size()!=32||out.size()!=32)return S::InvalidArgument();
  EVP_PKEY* key=EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519,nullptr,priv.data(),32);
  EVP_PKEY* remote=EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519,nullptr,peer.data(),32);
  EVP_PKEY_CTX* ctx=key?EVP_PKEY_CTX_new(key,nullptr):nullptr;size_t len=32;
  bool ok=ctx&&remote&&EVP_PKEY_derive_init(ctx)==1&&EVP_PKEY_derive_set_peer(ctx,remote)==1&&EVP_PKEY_derive(ctx,out.data(),&len)==1&&len==32;
  unsigned char nonzero=0;for(size_t i=0;i<out.size();i++)nonzero|=out[i];ok=ok&&nonzero;
  EVP_PKEY_CTX_free(ctx);EVP_PKEY_free(key);EVP_PKEY_free(remote);
  if(!ok)OPENSSL_cleanse(out.data(),out.size());return ok?S::Ok():S::Unauthenticated();
 }
 S Sha256(C data,B out) noexcept override {return Sha256Concat(data,{},out);}
 S Sha256Concat(C a,C b,B out) noexcept override {
  if(out.size()!=32)return S::InvalidArgument();SHA256_CTX ctx;
  bool ok=SHA256_Init(&ctx)&&SHA256_Update(&ctx,a.data(),a.size())&&SHA256_Update(&ctx,b.data(),b.size())&&SHA256_Final(out.data(),&ctx);
  return result(ok);
 }
 S HkdfSha256(C salt,C ikm,C info,nc::Span<B> outputs) noexcept override {
  if(outputs.empty()||outputs.size()>3)return S::InvalidArgument();
  for(size_t i=0;i<outputs.size();i++)if(outputs[i].size()!=32)return S::InvalidArgument();
  // Noise Split uses empty IKM; OpenSSL 1.1's EVP HKDF rejects it.
  // RFC 5869 extract/expand works for empty input as well as DH material.
  unsigned char zeros[32]{},prk[32]{},previous[32]{},derived[96]{};unsigned length=0;
  bool ok=HMAC(EVP_sha256(),salt.empty()?zeros:salt.data(),salt.empty()?32:salt.size(),ikm.empty()?zeros:ikm.data(),ikm.size(),prk,&length)&&length==32;
  for(size_t i=0;ok&&i<outputs.size();i++){
   unsigned char counter=i+1;HMAC_CTX* ctx=HMAC_CTX_new();
   ok=ctx&&HMAC_Init_ex(ctx,prk,32,EVP_sha256(),nullptr)==1;
   if(ok&&i)ok=HMAC_Update(ctx,previous,32)==1;
   if(ok&&!info.empty())ok=HMAC_Update(ctx,info.data(),info.size())==1;
   if(ok)ok=HMAC_Update(ctx,&counter,1)==1&&HMAC_Final(ctx,previous,&length)==1&&length==32;
   if(ok)memcpy(derived+i*32,previous,32);HMAC_CTX_free(ctx);
  }
  if(ok)for(size_t i=0;i<outputs.size();i++)memcpy(outputs[i].data(),derived+i*32,32);
  OPENSSL_cleanse(prk,sizeof(prk));OPENSSL_cleanse(previous,sizeof(previous));OPENSSL_cleanse(derived,sizeof(derived));return result(ok);
 }
 S Aes256GcmSeal(C key,C nonce,C aad,C plain,B out) noexcept override {
  if(key.size()!=32||nonce.size()!=12||out.size()<plain.size()+16)return S::InvalidArgument();
  EVP_CIPHER_CTX* ctx=EVP_CIPHER_CTX_new();int n=0,total=0;
  bool ok=ctx&&EVP_EncryptInit_ex(ctx,EVP_aes_256_gcm(),nullptr,key.data(),nonce.data())==1;
  if(ok&&!aad.empty())ok=EVP_EncryptUpdate(ctx,nullptr,&n,aad.data(),aad.size())==1;
  if(ok&&!plain.empty()){ok=EVP_EncryptUpdate(ctx,out.data(),&n,plain.data(),plain.size())==1;total=n;}
  if(ok)ok=EVP_EncryptFinal_ex(ctx,out.data()+total,&n)==1&&EVP_CIPHER_CTX_ctrl(ctx,EVP_CTRL_GCM_GET_TAG,16,out.data()+plain.size())==1;
  EVP_CIPHER_CTX_free(ctx);return result(ok);
 }
 S Aes256GcmOpen(C key,C nonce,C aad,C cipher,B out) noexcept override {
  if(key.size()!=32||nonce.size()!=12||cipher.size()<16||out.size()<cipher.size()-16)return S::InvalidArgument();
  EVP_CIPHER_CTX* ctx=EVP_CIPHER_CTX_new();int n=0,total=0;unsigned char empty[16]{};auto dst=out.empty()?empty:out.data();size_t len=cipher.size()-16;
  bool ok=ctx&&EVP_DecryptInit_ex(ctx,EVP_aes_256_gcm(),nullptr,key.data(),nonce.data())==1;
  if(ok&&!aad.empty())ok=EVP_DecryptUpdate(ctx,nullptr,&n,aad.data(),aad.size())==1;
  if(ok&&len){ok=EVP_DecryptUpdate(ctx,dst,&n,cipher.data(),len)==1;total=n;}
  if(ok)ok=EVP_CIPHER_CTX_ctrl(ctx,EVP_CTRL_GCM_SET_TAG,16,const_cast<unsigned char*>(cipher.data()+len))==1&&EVP_DecryptFinal_ex(ctx,dst+total,&n)==1;
  EVP_CIPHER_CTX_free(ctx);if(!ok){OPENSSL_cleanse(dst,len);return S::Unauthenticated();}return S::Ok();
 }
};
