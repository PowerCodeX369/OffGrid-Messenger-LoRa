#pragma once
#include <stdint.h>
#include <stddef.h>
typedef int psa_status_t;
typedef uint32_t psa_key_id_t;
typedef struct {unsigned dummy;} psa_key_attributes_t;
#define PSA_SUCCESS 0
#define PSA_KEY_ATTRIBUTES_INIT {0}
#define PSA_KEY_TYPE_HMAC 1
#define PSA_KEY_USAGE_SIGN_MESSAGE 1
#define PSA_ALG_SHA_256 256
#define PSA_ALG_HMAC(x) (x)
void psa_set_key_type(psa_key_attributes_t*,int);
void psa_set_key_bits(psa_key_attributes_t*,size_t);
void psa_set_key_usage_flags(psa_key_attributes_t*,unsigned);
void psa_set_key_algorithm(psa_key_attributes_t*,int);
void psa_reset_key_attributes(psa_key_attributes_t*);
psa_status_t psa_crypto_init(void);
psa_status_t psa_import_key(psa_key_attributes_t*,const uint8_t*,size_t,psa_key_id_t*);
psa_status_t psa_mac_compute(psa_key_id_t,int,const uint8_t*,size_t,uint8_t*,size_t,size_t*);
psa_status_t psa_destroy_key(psa_key_id_t);
