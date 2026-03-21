#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct lore_api_result
{
	long status_code;
	char *body_text;
	int ok;
	char *error_text;
} lore_api_result;

typedef void (*lore_api_callback)(const lore_api_result *result, void *user_data);

/*
 * Example simple GET call.
 */
const char* random_hex_32();

lore_api_result lore_get(lore_api_callback callback);

lore_api_result lore_deviceLogin(
	const char *hexID, 
	lore_api_callback callback);

lore_api_result lore_deviceLoginCheck(
	const char *hexID, 
	const char *stateID,
	lore_api_callback callback);

lore_api_result lore_deviceLoginCheckExchangeCode(
	const char *code,
	lore_api_callback callback);

lore_api_result lore_authGetBearer(
	const char *hexID, 
	const char *state,
	lore_api_callback callback);

lore_api_result lore_keys(
	const char *bearer,
	lore_api_callback callback);

lore_api_result lore_keys_generate(
	const char *bearer,
	lore_api_callback callback);

lore_api_result lore_keys_getPublic(
	const char *bearer,
	const char *keyId,
	lore_api_callback callback);

lore_api_result lore_keys_deletePublic(
	const char *bearer,
	const char *keyId,
	lore_api_callback callback);

lore_api_result lore_broadcastsCreate(
	const char *bearer, const char *metadataJson,
	lore_api_callback callback,
	void *user_data);

lore_api_result lore_broadcastsGet(
	const char *bearer,
	const char *keyId,
	lore_api_callback callback,
	void *user_data);

lore_api_result lore_broadcastsDelete(
	const char *bearer,
	const char *keyId,
	lore_api_callback callback,
	void *user_data);

lore_api_result lore_broadcastsUpdate(
	const char *bearer,
	const char *keyId,
	const char *jsonData,
	lore_api_callback callback);

bool lore_authConfirm(
	const char *bearer);
	
/*
 * Free strings allocated inside a lore_api_result.
 */
void lore_api_free_result(lore_api_result *result);

#ifdef __cplusplus
}
#endif

// end of file
