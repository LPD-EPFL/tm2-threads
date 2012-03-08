/* 
 * File:   log.c
 * Author: trigonak
 *
 * Created on June 01, 2011, 6:33PM
 */

#include "log.h"

inline write_set_t * write_set_new() {
    write_set_t *write_set;

    if ((write_set = (write_set_t *) malloc(sizeof (write_set_t))) == NULL) {
        PRINTD("Could not initialize the write set");
        return NULL;
    }

    if ((write_set->write_entries = (write_entry_t *) malloc(WRITE_SET_SIZE * sizeof (write_entry_t))) == NULL) {
        free(write_set);
        PRINTD("Could not initialize the write set");
        return NULL;
    }

    write_set->nb_entries = 0;
    write_set->size = WRITE_SET_SIZE;

    return write_set;
}

inline void write_set_free(write_set_t *write_set) {
    free(write_set->write_entries);
    free(write_set);
}

inline write_set_t * write_set_empty(write_set_t *write_set) {

    if (write_set->size > WRITE_SET_SIZE) {
        write_entry_t * temp;
        if ((temp = (write_entry_t *) realloc(write_set->write_entries, WRITE_SET_SIZE * sizeof (write_entry_t))) == NULL) {
            free(write_set->write_entries);
            PRINT("realloc @ write_set_empty failed");
            write_set->write_entries = (write_entry_t *) malloc(WRITE_SET_SIZE * sizeof (write_entry_t));
            if (write_set->write_entries == NULL) {
                PRINT("malloc write_set->write_entries @ write_set_empty");
                return NULL;
            }
        }
    }
    write_set->size = WRITE_SET_SIZE;
    write_set->nb_entries = 0;
    return write_set;
}

inline write_entry_t * write_set_entry(write_set_t *write_set) {
    if (write_set->nb_entries == write_set->size) {
        //PRINTD("WRITE set max sized (%d)", write_set->size);
        unsigned int new_size = 2 * write_set->size;
        write_entry_t *temp;
        if ((temp = (write_entry_t *) realloc(write_set->write_entries, new_size * sizeof (write_entry_t))) == NULL) {
            write_set_free(write_set);
            PRINTD("Could not resize the write set");
            return NULL;
        }

        write_set->write_entries = temp;
        write_set->size = new_size;
    }

    return &write_set->write_entries[write_set->nb_entries++];
}

inline void write_entry_set_value(write_entry_t *we, void *value) {
    switch (we->datatype) {
        case TYPE_CHAR:
            we->c = CAST_CHAR(value);
            break;
        case TYPE_DOUBLE:
            we->d = CAST_DOUBLE(value);
            break;
        case TYPE_FLOAT:
            we->f = CAST_FLOAT(value);
            break;
        case TYPE_INT:
            we->i = CAST_INT(value);
            break;
        case TYPE_LONG:
            we->li = CAST_LONG(value);
            break;
        case TYPE_LONGLONG:
            we->lli = CAST_LONGLONG(value);
            break;
        case TYPE_SHORT:
            we->s = CAST_SHORT(value);
            break;
        case TYPE_UCHAR:
            we->uc = CAST_UCHAR(value);
            break;
        case TYPE_UINT:
            we->ui = CAST_UINT(value);
            break;
        case TYPE_ULONG:
            we->uli = CAST_ULONG(value);
            break;
        case TYPE_ULONGLONG:
            we->ulli = CAST_ULONGLONG(value);
            break;
        case TYPE_USHORT:
            we-> us = CAST_USHORT(value);
            break;
        case TYPE_POINTER:
            we->p = value;
            break;
        default: //pointer to a mem area that the we->datatype defines the length
            we->p = value;
    }
}

inline void write_set_insert(write_set_t *write_set, DATATYPE datatype, void *value, tm_intern_addr_t address) {
    write_entry_t *we = write_set_entry(write_set);

    we->datatype = datatype;
    we->address = address;
    write_entry_set_value(we, value);
}

inline void write_set_update(write_set_t *write_set, DATATYPE datatype, void *value, tm_intern_addr_t address) {
    unsigned int i;
    for (i = 0; i < write_set->nb_entries; i++) {
        if (write_set->write_entries[i].address == address) {
            write_entry_set_value(&write_set->write_entries[i], value);
            return;
        }
    }

    write_set_insert(write_set, datatype, value, address);
}

inline void write_entry_persist(write_entry_t *we) {
    switch (we->datatype) {
        case TYPE_CHAR:
            CAST_CHAR(we->address) = we->c;
            break;
        case TYPE_DOUBLE:
            CAST_DOUBLE(we->address) = we->d;
            break;
        case TYPE_FLOAT:
            CAST_FLOAT(we->address) = we->f;
            break;
        case TYPE_INT:
            CAST_INT(we->address) = we->i;
            break;
        case TYPE_LONG:
            CAST_LONG(we->address) = we->li;
            break;
        case TYPE_LONGLONG:
            CAST_LONGLONG(we->address) = we->lli;
            break;
        case TYPE_SHORT:
            CAST_SHORT(we->address) = we->s;
            break;
        case TYPE_UCHAR:
            CAST_UCHAR(we->address) = we->uc;
            break;
        case TYPE_UINT:
            CAST_UINT(we->address) = we->ui;
            break;
        case TYPE_ULONG:
            CAST_ULONG(we->address) = we->uli;
            break;
        case TYPE_ULONGLONG:
            CAST_ULONGLONG(we->address) = we->ulli;
            break;
        case TYPE_USHORT:
            CAST_USHORT(we->address) = we->us;
            break;
        case TYPE_POINTER:
            we->address = (tm_intern_addr_t)we->p;
            break;
        default:
            memcpy(we->address, we->p, we->datatype);
    }
}

inline void write_entry_print(write_entry_t *we) {
    switch (we->datatype) {
        case TYPE_CHAR:
            PRINTSME("[%"PRIxIA" :  %c]", (we->address), we->c);
            break;
        case TYPE_DOUBLE:
            PRINTSME("[%"PRIxIA" :  %f]", (we->address), we->d);
            break;
        case TYPE_FLOAT:
            PRINTSME("[%"PRIxIA" :  %f]", (we->address), we->f);
            break;
        case TYPE_INT:
            PRINTSME("[%"PRIxIA" :  %d]", (we->address), we->i);
            break;
        case TYPE_LONG:
            PRINTSME("[%"PRIxIA" :  %ld]", (we->address), we->li);
            break;
        case TYPE_LONGLONG:
            PRINTSME("[%"PRIxIA" :  %lld]", (we->address), we->lli);
            break;
        case TYPE_SHORT:
            PRINTSME("[%"PRIxIA" :  %i]", (we->address), we->s);
            break;
        case TYPE_UCHAR:
            PRINTSME("[%"PRIxIA" :  %c]", (we->address), we->uc);
            break;
        case TYPE_UINT:
            PRINTSME("[%"PRIxIA" :  %u]", (we->address), we->ui);
            break;
        case TYPE_ULONG:
            PRINTSME("[%"PRIxIA" :  %lu]", (we->address), we->uli);
            break;
        case TYPE_ULONGLONG:
            PRINTSME("[%"PRIxIA" :  %llu]", (we->address), we->ulli);
            break;
        case TYPE_USHORT:
            PRINTSME("[%"PRIxIA" :  %us]", (we->address), we->us);
            break;
        case TYPE_POINTER:
            PRINTSME("[%"PRIxIA" :  %p]", we->address, we->p);
            break;
        default:
            PRINTSME("[%"PRIxIA" :  %s]", (char *) we->address, (const char *) we->p);
    }
}

inline void write_set_print(write_set_t *write_set) {
    PRINTSME("WRITE SET (elements: %d, size: %d) --------------\n", write_set->nb_entries, write_set->size);
    unsigned int i;
    for (i = 0; i < write_set->nb_entries; i++) {
        write_entry_print(&write_set->write_entries[i]);
    }
    FLUSH
}

inline void write_set_persist(write_set_t *write_set) {
    unsigned int i;
    for (i = 0; i < write_set->nb_entries; i++) {
        write_entry_persist(&write_set->write_entries[i]);
    }
}

inline write_entry_t * write_set_contains(write_set_t *write_set, tm_intern_addr_t address) {
    unsigned int i;
    for (i = write_set->nb_entries; i-- > 0; ) {
        if (write_set->write_entries[i].address == address) {
            return &write_set->write_entries[i];
        }
    }

    return NULL;
}

/*______________________________________________________________________________________________________
 * READ SET                                                                                             |
 *______________________________________________________________________________________________________|
 */


inline read_set_t * read_set_new() {
    read_set_t *read_set;

    if ((read_set = (read_set_t *) malloc(sizeof (read_set_t))) == NULL) {
        PRINTD("Could not initialize the read set");
        return NULL;
    }

    if ((read_set->read_entries = (read_entry_l_t *) malloc(READ_SET_SIZE * sizeof (read_entry_l_t))) == NULL) {
        free(read_set);
        PRINTD("Could not initialize the read set");
        return NULL;
    }

    read_set->nb_entries = 0;
    read_set->size = READ_SET_SIZE;

    return read_set;
}

inline void read_set_free(read_set_t *read_set) {
    free(read_set->read_entries);
    free(read_set);
}

inline read_set_t * read_set_empty(read_set_t *read_set) {

    if (read_set->size > READ_SET_SIZE) {
        read_entry_l_t * temp;
        if ((temp = (read_entry_l_t *) realloc(read_set->read_entries, READ_SET_SIZE * sizeof (read_entry_l_t))) == NULL) {
            free(read_set->read_entries);
            PRINT("realloc @ read_set_empty failed");
            read_set->read_entries = (read_entry_l_t *) malloc(READ_SET_SIZE * sizeof (read_entry_l_t));
            if (read_set->read_entries == NULL) {
                PRINT("malloc read_set->read_entries @ read_set_empty");
                return NULL;
            }
        }
    }
    read_set->size = READ_SET_SIZE;
    read_set->nb_entries = 0;
    return read_set;
}

inline read_entry_l_t * read_set_entry(read_set_t *read_set) {
    if (read_set->nb_entries == read_set->size) {
        unsigned int new_size = 2 * read_set->size;
        //PRINTD("READ set max sized (%d)", read_set->size);
        read_entry_l_t *temp;
        if ((temp = (read_entry_l_t *) realloc(read_set->read_entries, new_size * sizeof (read_entry_l_t))) == NULL) {
            PRINTD("Could not resize the write set");
            read_set_free(read_set);
            return NULL;
        }

        read_set->read_entries = temp;
        read_set->size = new_size;
    }

    return &read_set->read_entries[read_set->nb_entries++];
}

#ifdef READDATATYPE

inline void read_set_insert(read_set_t *read_set, DATATYPE datatype, tm_intern_addr_t address) {
#else

inline void read_set_insert(read_set_t *read_set, tm_intern_addr_t address) {
#endif
    read_entry_l_t *re = read_set_entry(read_set);
#ifdef READDATATYPE
    re->datatype = datatype;
#endif
    re->address = address;
}

#ifdef READDATATYPE

inline BOOLEAN read_set_update(read_set_t *read_set, DATATYPE datatype, tm_intern_addr_t address) {
#else

inline BOOLEAN read_set_update(read_set_t *read_set, tm_intern_addr_t address) {
#endif
    unsigned int i;
    for (i = 0; i < read_set->nb_entries; i++) {
        if (read_set->read_entries[i].address == address) {
            return TRUE;
        }
    }
#ifdef READDATATYPE
    read_set_insert(read_set, datatype, address);
#else
    read_set_insert(read_set, address);
#endif
    return FALSE;
}

inline read_entry_l_t * read_set_contains(read_set_t *read_set, tm_intern_addr_t address) {
    unsigned int i;
    for (i = read_set->nb_entries; i-- > 0;) {
        if (read_set->read_entries[i].address == address) {
            return &read_set->read_entries[i];
        }
    }

    return NULL;
}


#ifdef PGAS

/*______________________________________________________________________________________________________
 * WRITE SET PGAS                                                                                       |
 *______________________________________________________________________________________________________|
 */


inline write_set_pgas_t * write_set_pgas_new() {
    write_set_pgas_t *write_set_pgas;

    if ((write_set_pgas = (write_set_pgas_t *) malloc(sizeof (write_set_pgas_t))) == NULL) {
        PRINT("Could not initialize the write set");
        return NULL;
    }

    if ((write_set_pgas->write_entries = (write_entry_pgas_t *) malloc(WRITE_SET_PGAS_SIZE * sizeof (write_entry_pgas_t))) == NULL) {
        free(write_set_pgas);
        PRINT("Could not initialize the write set");
        return NULL;
    }

    write_set_pgas->nb_entries = 0;
    write_set_pgas->size = WRITE_SET_PGAS_SIZE;

    return write_set_pgas;
}

inline void write_set_pgas_free(write_set_pgas_t *write_set_pgas) {
    free(write_set_pgas->write_entries);
    free(write_set_pgas);
}

inline write_set_pgas_t * write_set_pgas_empty(write_set_pgas_t *write_set_pgas) {

#ifdef WRITE_SET_RESIZE
    if (write_set_pgas->size > WRITE_SET_PGAS_SIZE) {
        write_entry_pgas_t * temp;
        if ((temp = (write_entry_pgas_t *) realloc(write_set_pgas->write_entries, WRITE_SET_PGAS_SIZE * sizeof (write_entry_pgas_t))) == NULL) {
            free(write_set_pgas->write_entries);
            PRINT("realloc @ write_set_pgas_empty failed");
            write_set_pgas->write_entries = (write_entry_pgas_t *) malloc(WRITE_SET_PGAS_SIZE * sizeof (write_entry_pgas_t));
            if (write_set_pgas->write_entries == NULL) {
                PRINT("malloc write_set_pgas->write_entries @ write_set_pgas_empty");
                return NULL;
            }
        }
    }
    write_set_pgas->size = WRITE_SET_PGAS_SIZE;
#endif
    write_set_pgas->nb_entries = 0;
    return write_set_pgas;
}

inline write_entry_pgas_t * write_set_pgas_entry(write_set_pgas_t *write_set_pgas) {
    if (write_set_pgas->nb_entries == write_set_pgas->size) {
        unsigned int new_size = 2 * write_set_pgas->size;
        PRINT("WRITE set max sized (%d)(%d)", write_set_pgas->size, new_size);
        write_entry_pgas_t *temp;
        if ((temp = (write_entry_pgas_t *) realloc(write_set_pgas->write_entries, new_size * sizeof (write_entry_pgas_t))) == NULL) {
            PRINT("Could not resize the write set pgas");
            write_set_pgas_free(write_set_pgas);
            return NULL;
        }

        write_set_pgas->write_entries = temp;
        write_set_pgas->size = new_size;
    }
    
    return &write_set_pgas->write_entries[write_set_pgas->nb_entries++];
}

inline void write_set_pgas_insert(write_set_pgas_t *write_set_pgas, int value, tm_intern_addr_t address) {
    write_entry_pgas_t *we = write_set_pgas_entry(write_set_pgas);

    we->address = address;
    we->value = value;
}

inline void write_set_pgas_update(write_set_pgas_t *write_set_pgas, int value, tm_intern_addr_t address) {
    unsigned int i;
    for (i = 0; i < write_set_pgas->nb_entries; i++) {
        if (write_set_pgas->write_entries[i].address == address) {
            write_set_pgas->write_entries[i].value = value;
            return;
        }
    }

    write_set_pgas_insert(write_set_pgas, value, address);
}

inline void write_entry_pgas_persist(write_entry_pgas_t *we) {
    PGAS_write(we->address, we->value);
}

inline void write_entry_pgas_print(write_entry_pgas_t *we) {
    PRINTSME("[%5d :  %d]", (we->address), we->value);
}

inline void write_set_pgas_print(write_set_pgas_t *write_set_pgas) {
    PRINTSME("WRITE SET PGAS (elements: %d, size: %d) --------------\n", write_set_pgas->nb_entries, write_set_pgas->size);
    unsigned int i;
    for (i = 0; i < write_set_pgas->nb_entries; i++) {
        write_entry_pgas_print(&write_set_pgas->write_entries[i]);
    }
    FLUSH
}

inline void write_set_pgas_persist(write_set_pgas_t *write_set_pgas) {
    unsigned int i;
    for (i = 0; i < write_set_pgas->nb_entries; i++) {
        write_entry_pgas_persist(&write_set_pgas->write_entries[i]);
    }
}

inline write_entry_pgas_t* write_set_pgas_contains(write_set_pgas_t *write_set_pgas, tm_intern_addr_t address) {
    unsigned int i;
    for (i = write_set_pgas->nb_entries; i-- > 0;) {
        if (write_set_pgas->write_entries[i].address == address) {
            return &write_set_pgas->write_entries[i];
        }
    }

    return NULL;
}

#endif
