#include "index.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <inttypes.h>

// PROVIDED
IndexEntry* index_find(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++)
        if (strcmp(index->entries[i].path, path) == 0)
            return &index->entries[i];
    return NULL;
}

int index_remove(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0) {
            int rem = index->count - i - 1;
            if (rem > 0)
                memmove(&index->entries[i], &index->entries[i+1], rem * sizeof(IndexEntry));
            index->count--;
            return index_save(index);
        }
    }
    fprintf(stderr, "error: '%s' is not in the index\n", path);
    return -1;
}

int index_status(const Index *index) {
    printf("Staged changes:\n");
    int sc = 0;
    for (int i = 0; i < index->count; i++) {
        printf("  staged:     %s\n", index->entries[i].path);
        sc++;
    }
    if (!sc) printf("  (nothing to show)\n");
    printf("\nUnstaged changes:\n");
    int uc = 0;
    for (int i = 0; i < index->count; i++) {
        struct stat st;
        if (stat(index->entries[i].path, &st) != 0) {
            printf("  deleted:    %s\n", index->entries[i].path);
            uc++;
        } else if (st.st_mtime != (time_t)index->entries[i].mtime_sec ||
                   st.st_size != (off_t)index->entries[i].size) {
            printf("  modified:   %s\n", index->entries[i].path);
            uc++;
        }
    }
    if (!uc) printf("  (nothing to show)\n");
    printf("\nUntracked files:\n");
    int ut = 0;
    DIR *d = opendir(".");
    if (d) {
        struct dirent *e;
        while ((e = readdir(d))) {
            if (e->d_name[0] == '.') continue;
            if (strcmp(e->d_name, "pes") == 0) continue;
            int tracked = 0;
            for (int i = 0; i < index->count; i++)
                if (strcmp(index->entries[i].path, e->d_name) == 0) { tracked = 1; break; }
            if (!tracked) { printf("  untracked:  %s\n", e->d_name); ut++; }
        }
        closedir(d);
    }
    if (!ut) printf("  (nothing to show)\n");
    printf("\n");
    return 0;
}

int index_load(Index *index) {
    FILE *f = fopen(".pes/index", "r");
    if (!f) { index->count = 0; return 0; }
    index->count = 0;
    while (index->count < MAX_INDEX_ENTRIES) {
        IndexEntry *e = &index->entries[index->count];
        char hx[65];
        int matched = fscanf(f, "%o %64s %" SCNu64 " %" SCNu64 " %[^\n]\n",
                             &e->mode, hx, &e->mtime_sec, &e->size, e->path);
        if (matched == EOF) break;
        if (matched != 5) { fclose(f); return -1; }
        if (hex_to_hash(hx, &e->hash) != 0) { fclose(f); return -1; }
        index->count++;
    }
    fclose(f);
    return 0;
}

static int cmp(const void *a, const void *b) {
    return strcmp(((const IndexEntry*)a)->path, ((const IndexEntry*)b)->path);
}

int index_save(const Index *index) {
    Index s = *index;
    qsort(s.entries, s.count, sizeof(IndexEntry), cmp);
    FILE *f = fopen(".pes/index.tmp", "w");
    if (!f) return -1;
    for (int i = 0; i < s.count; i++) {
        char hx[65];
        hash_to_hex(&s.entries[i].hash, hx);
        fprintf(f, "%o %s %" PRIu64 " %" PRIu64 " %s\n",
                s.entries[i].mode, hx, s.entries[i].mtime_sec, s.entries[i].size, s.entries[i].path);
    }
    fflush(f);
    int fd = fileno(f);
    if (fd != -1) fsync(fd);
    fclose(f);
    if (rename(".pes/index.tmp", ".pes/index") != 0) return -1;
    return 0;
}

int index_add(Index *index, const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) { perror("stat"); return -1; }
    if (!S_ISREG(st.st_mode)) {
        fprintf(stderr, "error: '%s' is not a regular file\n", path);
        return -1;
    }
    FILE *f = fopen(path, "rb");
    if (!f) { perror("fopen"); return -1; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    unsigned char *buf = malloc(sz);
    if (!buf) { fclose(f); return -1; }
    size_t rd = fread(buf, 1, sz, f);
    fclose(f);
    if (rd != (size_t)sz) { free(buf); return -1; }

    ObjectID oid;
    if (object_write(OBJ_BLOB, buf, sz, &oid) != 0) {
        free(buf);
        fprintf(stderr, "error: failed to write blob for %s\n", path);
        return -1;
    }
    free(buf);

    IndexEntry *e = index_find(index, path);
    if (!e) {
        if (index->count >= MAX_INDEX_ENTRIES) {
            fprintf(stderr, "error: index full\n");
            return -1;
        }
        e = &index->entries[index->count++];
        strncpy(e->path, path, 511);
        e->path[511] = '\0';
    }
    e->mode = st.st_mode & 0777;
    e->mtime_sec = st.st_mtime;
    e->size = st.st_size;
    e->hash = oid;
    return index_save(index);
}
