/*
 * Copyright (c) 2024 Nuvoton technology corporation
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#ifndef _CRYPTO_IOCTL_H_
#define _CRYPTO_IOCTL_H_

#define NVT_ECC          "/dev/nuvoton-ecc"
#define NVT_RSA          "/dev/nuvoton-rsa"

typedef unsigned char   u8;

#define CRYPTO_IOC_MAGIC	'C'
#define RSA_IOC_SET_BITLEN	_IOW(CRYPTO_IOC_MAGIC, 20, unsigned long)
#define RSA_IOC_SET_N		_IOW(CRYPTO_IOC_MAGIC, 21, u8 *)
#define RSA_IOC_SET_E		_IOW(CRYPTO_IOC_MAGIC, 22, u8 *)
#define RSA_IOC_SET_M		_IOW(CRYPTO_IOC_MAGIC, 23, u8 *)
#define RSA_IOC_SET_P		_IOW(CRYPTO_IOC_MAGIC, 24, u8 *)
#define RSA_IOC_SET_Q		_IOW(CRYPTO_IOC_MAGIC, 25, u8 *)
#define RSA_IOC_RUN		_IOW(CRYPTO_IOC_MAGIC, 29, u8 *)

#define ECC_IOC_KEY_GEN           _IOW(CRYPTO_IOC_MAGIC, 53, u8 *)
#define ECC_IOC_POINT_MUL         _IOW(CRYPTO_IOC_MAGIC, 55, u8 *)
#define ECC_IOC_SIG_GEN           _IOW(CRYPTO_IOC_MAGIC, 57, u8 *)
#define ECC_IOC_SIG_VERIFY        _IOW(CRYPTO_IOC_MAGIC, 58, u8 *)

enum {
	CURVE_P_192  = 0x01,
	CURVE_P_224  = 0x02,
	CURVE_P_256  = 0x03,
	CURVE_P_384  = 0x04,
	CURVE_P_521  = 0x05,
	CURVE_K_163  = 0x11,
	CURVE_K_233  = 0x12,
	CURVE_K_283  = 0x13,
	CURVE_K_409  = 0x14,
	CURVE_K_571  = 0x15,
	CURVE_B_163  = 0x21,
	CURVE_B_233  = 0x22,
	CURVE_B_283  = 0x23,
	CURVE_B_409  = 0x24,
	CURVE_B_571  = 0x25,
	CURVE_KO_192 = 0x31,
	CURVE_KO_224 = 0x32,
	CURVE_KO_256 = 0x33,
	CURVE_BP_256 = 0x41,
	CURVE_BP_384 = 0x42,
	CURVE_BP_512 = 0x43,
	CURVE_SM2_256 = 0x50,
	CURVE_25519  = 0x51,
	CURVE_UNDEF,
};

#define ECC_KMAXL                 160

struct ecc_args_t {
	int	curve;
	int	keylen;
					/* Key Store key is OTP key if bit7 is 1, otherwise is SRAM key. */
					/* For example, 0x84 is OTP key 4, while 0x11 is SRAM key 17 */
	int	knum_d;			/* Key Store number of private key; -1 means unused */
	int	knum_x;			/* Key Store number of public key X; -1 means unused */
	int	knum_y;			/* Key Store number of public key Y; -1 means unused */
	u8	d[ECC_KMAXL];		/* private key (not used if select Key Store key) */
	u8	Qx[ECC_KMAXL];		/* public key X (not used if select Key Store key) */
	u8	Qy[ECC_KMAXL];		/* public key Y (not used if select Key Store key) */
	u8	sha_dgst[ECC_KMAXL];	/* sha digest of the message(or image) to be verified */
	u8	k[ECC_KMAXL];		/* used for signature generation */
	u8	R[ECC_KMAXL];		/* ECDSA signature R */
	u8	S[ECC_KMAXL];		/* ECDSA signature S */
	u8	out_x[ECC_KMAXL];	/* output X */
	u8	out_y[ECC_KMAXL];	/* output Y */
};

void cstr_to_hex(u8 *cstr, u8 *hex_buff, int klen);
void hex_to_cstr(u8 *hex_buff, int len, u8 *cstr);
int ecc_public_key_generation(void);
int ecc_signature_verification(void);
int ecc_signature_generation(void);


#endif /* _CRYPTO_IOCTL_H_ */