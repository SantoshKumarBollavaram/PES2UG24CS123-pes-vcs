#include "pes.h"
#include "object.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <openssl/sha.h>

int object_write(ObjectType type, const unsigned char *data, size_t size, ObjectID *out_id) {
    const char *type_str = (type == OBJ_BLOB) ? "blob" :
                           (type == OBJ_TREE) ? "tree" : "commit";

    char header[256];
    int header_len = snprintf(header, sizeof(header), "%s %zu", type_str, size);
    if (header_len < 0 || header_len >= (int)sizeof(header)) return -1;

    size_t total_len = header_len + 1 + size;
    unsigned char *full = malloc(total_len);
    if (!full) return -1;
    memcpy(full, header, header_len);
    full[header_len] = '\0';
    memcpy(full + header_len + 1, data, size);

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(full, total_len, hash);
    memcpy(out_id->hash, hash, SHA256_DIGEST_LENGTH);

    char hash_hex[65] = {0};
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        sprintf(hash_hex + i*2, "%02x", hash[i]);

    char dir_path[256];
    snprintf(dir_path, sizeof(dir_path), ".pes/objects/%.2s", hash_hex);
    mkdir(dir_path, 0755);

    char obj_path[512];
    snprintf(obj_path, sizeof(obj_path), "%s/%s", dir_path, hash_hex + 2);

    FILE *f = fopen(obj_path, "wb");
    if (!f) { free(full); return -1; }
    fwrite(full, 1, total_len, f);
    fclose(f);
    free(full);
    return 0;
}

int hex_to_hash(const char *hex, ObjectID *id_out) {
    for (int i = 0; i < HASH_SIZE; i++)
        sscanf(hex + i*2, "%2hhx", &id_out->hash[i]);
    return 0;
}

void hash_to_hex(const ObjectID *id, char *hex_out) {
    for (int i = 0; i < HASH_SIZE; i++)
        sprintf(hex_out + i*2, "%02x", id->hash[i]);
    hex_out[64] = '\0';
}
