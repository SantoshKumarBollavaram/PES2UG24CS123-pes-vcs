#ifndef OBJECT_H
#define OBJECT_H
#include "pes.h"
int object_write(ObjectType type, const unsigned char *data, size_t size, ObjectID *out_id);
int object_read(const ObjectID *id, ObjectType *type, unsigned char **data, size_t *size);
#endif
