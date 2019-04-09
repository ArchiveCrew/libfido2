/*
 * Copyright (c) 2018 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/obj_mac.h>

#include <string.h>
#include "fido.h"
#include "fido/eddsa.h"

static int
decode_coord(const cbor_item_t *item, void *xy, size_t xy_len)
{
	if (cbor_isa_bytestring(item) == false ||
	    cbor_bytestring_is_definite(item) == false ||
	    cbor_bytestring_length(item) != xy_len) {
		log_debug("%s: cbor type", __func__);
		return (-1);
	}

	memcpy(xy, cbor_bytestring_handle(item), xy_len);

	return (0);
}

static int
decode_pubkey_point(const cbor_item_t *key, const cbor_item_t *val, void *arg)
{
	eddsa_pk_t *k = arg;

	if (cbor_isa_negint(key) == false ||
	    cbor_int_get_width(key) != CBOR_INT_8)
		return (0); /* ignore */

	switch (cbor_get_uint8(key)) {
	case 1: /* x coordinate */
		return (decode_coord(val, &k->x, sizeof(k->x)));
	}

	return (0); /* ignore */
}

int
eddsa_pk_decode(const cbor_item_t *item, eddsa_pk_t *k)
{
	if (cbor_isa_map(item) == false ||
	    cbor_map_is_definite(item) == false ||
	    cbor_map_iter(item, k, decode_pubkey_point) < 0) {
		log_debug("%s: cbor type", __func__);
		return (-1);
	}

	return (0);
}

cbor_item_t *
eddsa_pk_encode(const eddsa_pk_t *pk)
{
	cbor_item_t		*item = NULL;
	struct cbor_pair	 pair;

	if ((item = cbor_new_definite_map(4)) == NULL)
		goto fail;

	pair.key = cbor_move(cbor_build_uint8(1));
	pair.value = cbor_move(cbor_build_uint8(1));
	if (!cbor_map_add(item, pair))
		goto fail;

	pair.key = cbor_move(cbor_build_uint8(3));
	pair.value = cbor_move(cbor_build_negint8(-COSE_EDDSA - 1));
	if (!cbor_map_add(item, pair))
		goto fail;

	pair.key = cbor_move(cbor_build_negint8(0));
	pair.value = cbor_move(cbor_build_uint8(6));
	if (!cbor_map_add(item, pair))
		goto fail;

	pair.key = cbor_move(cbor_build_negint8(1));
	pair.value = cbor_move(cbor_build_bytestring(pk->x, sizeof(pk->x)));
	if (!cbor_map_add(item, pair))
		goto fail;

	return (item);
fail:
	cbor_decref(&item);

	return (NULL);
}

eddsa_pk_t *
eddsa_pk_new(void)
{
	return (calloc(1, sizeof(eddsa_pk_t)));
}

void
eddsa_pk_free(eddsa_pk_t **pkp)
{
	eddsa_pk_t *pk;

	if (pkp == NULL || (pk = *pkp) == NULL)
		return;

	explicit_bzero(pk, sizeof(*pk));
	free(pk);

	*pkp = NULL;
}

int
eddsa_pk_from_ptr(eddsa_pk_t *pk, const void *ptr, size_t len)
{
	if (len < sizeof(*pk))
		return (FIDO_ERR_INVALID_ARGUMENT);

	memcpy(pk, ptr, sizeof(*pk));

	return (FIDO_OK);
}

int
eddsa_pk_set_x(eddsa_pk_t *pk, const unsigned char *x)
{
	memcpy(pk->x, x, sizeof(pk->x));

	return (0);
}

EVP_PKEY *
eddsa_pk_to_EVP_PKEY(const eddsa_pk_t *k)
{
	EVP_PKEY *pkey = NULL;

	if ((pkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL, k->x, sizeof(k->x))) == NULL) {
		log_debug("%s: EVP_PKEY raw public new", __func__);
		goto fail;
	}

fail:
	return (pkey);
}

int
eddsa_pk_from_EVP_PKEY(eddsa_pk_t *pk, const EVP_PKEY *pkey)
{
	int	ok = FIDO_ERR_INTERNAL;
	size_t	len = sizeof(pk->x);

	if (EVP_PKEY_get_raw_public_key(pkey, pk->x, &len) != 1) {
		goto fail;
	}

	ok = FIDO_OK;
fail:
	return (ok);
}
