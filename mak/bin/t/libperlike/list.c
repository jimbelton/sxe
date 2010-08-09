#include <assert.h>
#include <stdarg.h>
#include <stdio.h> /* DEBUG */
#include <stdlib.h>
#include <string.h>

#include "perlike.h"

struct List_impl
{
    List_type * array;
    unsigned    length;
    unsigned    offset;
    unsigned    size;
    char *      strdup;
};

List
List_clear(List list)
{
    if (list->array != NULL) {   
        free(list->array);
    }
    
    if (list->strdup != NULL) {   
        free(list->strdup);
    }
    
    memset(list, 0, sizeof(struct List_impl));
    return list;
}

static void
List_expand(List list, unsigned size)
{
    assert(list->array = realloc(list->array, size * sizeof(List_type)));
    list->size = size;
}

List
List_concat(List list, List list2)
{
    unsigned index  = list->offset + list->length;
    unsigned length = list->length + list2->length;
    unsigned i;
    
    if (list->offset + length > list->size) {
        List_expand(list, list->offset + length);
    }
    
    for (i = 0; i < list2->length; i++) {
        list->array[index + i] = list2->array[list2->offset + i];
    }

    list->length = length;
    return list;
}

void
List_delete(List list)
{
    List_clear(list);
    free(list);
}

unsigned
List_defined(List list, unsigned index)
{
    return index < list->length;
}

List_type
List_get(List list, unsigned index)
{
    assert(index < list->length);
    return list->array[list->offset + index];
}

List
List_grep(List list, unsigned (*predicate)(void *))
{
    unsigned i;
    unsigned end  = list->offset + list->length;
    List     grep = List_new();
    
    for (i = list->offset; i < end; i++) {
        if ((*predicate)(list->array[i])) {
            List_push(grep, list->array[i]);
        }
    }

    return grep;
}

unsigned
List_grep_length(List list, unsigned (*predicate)(void *))
{
    unsigned i;
    unsigned end = list->offset + list->length;
    unsigned len = 0;
    
    for (i = list->offset; i < end; i++) {
        if ((*predicate)(list->array[i])) {
            len++;
        }
    }

    return len;
}

char *
List_join_alloc(List list, char * delim)
{
    char *   join = NULL;
    unsigned i    = list->offset;
    unsigned end  = i + list->length;
    unsigned len  = 0;
    char *   str;
    unsigned add;
    unsigned dlen;
    
    for (; i < end; i++) {
        str = (char *)list->array[i];
        
        if ((str == NULL) || (str[0] == '\0')) {
            continue;
        }
        
        add = strlen(str);
        join = realloc(join, len + add + 1);

        if (len == 0) {
            dlen = strlen(delim);
        }
        else {
            strcpy(&join[len], delim);
            len += dlen;
        }
        
        strcpy(&join[len], str);
        len += add;
    }

    if (len == 0) {
        join  = malloc(1);
        *join = '\0';
    }

    return join;
}

char *
strn_copy(char * buf, const char * str, unsigned size)
{
    char * end = &buf[size - 1];
    
    /* Up until the end.
     */
    while (buf < end)
    {
        if ((*buf++ = *str++) == '\0') {
            return buf - 1;
        }
    }
    
    *buf = '\0';
    return NULL;
}

char *
List_join_strncpy(List list, char * delim, unsigned size, char * buf)
{
    char *   join = buf;
    unsigned i    = list->offset;
    unsigned end  = i + list->length;
    char *   str;
    unsigned add;
    unsigned len;

    for (; i < end; i++) {
        str = (char *)list->array[i];
        printf("1-%u/%u=%s%s %s\n", i, end, str, delim, join);
        
        if ((str == NULL) || (str[0] == '\0')) {
            continue;
        }
               
        if (join == buf) {
            len = strlen(delim);
        }
        else {
            if ((buf = strn_copy(buf, delim, size)) == NULL) {
                return join;
            }
            
            size -= len;
        }
        
        if ((buf = strn_copy(buf, str, size)) == NULL) {
            return join;
        }
         
        size -= strlen(str);
    }

    if (join == buf) {
        *join = '\0';
    }

    return join;
}

unsigned
List_length(List list)
{
    return list->length;
}

List
List_map(List list, void * (*func)(void *))
{
    unsigned i;
    unsigned end = list->offset + list->length;
    List     map = List_new();
    
    for (i = list->offset; i < end; i++) {
        List_push(map, (*func)(list->array[i]));
    }

    return map;
}

List
List_new(void)
{
    List list = calloc(1, sizeof(struct List_impl));   
    assert(list != NULL);
    return list;
}

List_type
List_pop(List list)
{
    assert(list->length > 0);
    return list->array[list->offset + --list->length];
}

unsigned
List_push(List list, List_type elem)
{
    unsigned index = list->offset + list->length;
    
    if (index == list->size) {
        List_expand(list, list->size + 1);
    }
    
    list->array[index] = elem;
    return ++list->length;
}

List
List_reverse(List list)
{
    unsigned i = list->offset;
    unsigned j = i + list->length - 1;
   
    while (i < j) {
        void * swap      = list->array[i];
        list->array[i++] = list->array[j];
        list->array[j--] = swap;
    }
    
    return list;
}

List_type
List_set(List list, unsigned index, List_type elem)
{
    index += list->offset;
    
    if (index >= list->size) {
        List_expand(list, index + 1);
    }
    
    if (index >= list->length) {
        list->length = index + 1;
    }
    
    list->array[index] = elem;
    return elem;
}

List_type
List_shift(List list)
{
    assert(list->length > 0);
    list->length--;
    return list->array[list->offset++];
}

List
List_sort(List list, int (*cmp)(const void *, const void *))
{
    if (list->length > 1) {
        qsort(&list->array[list->offset], list->length, sizeof(void *), cmp);
    }

    return list;
}

List
List_split_strdup(char * delim, char * str)
{
    unsigned len;
    char *   dup;
    List     list;

    assert(dup = malloc(len = strlen(str) + 1));
    strncpy(dup, str, len);
    list = List_split_strtok(delim, dup);
    list->strdup = dup;
    return list;
}

List
List_split_strncpy(char * delim, unsigned length, char * buf, char * str)
{
    strncpy(buf, str, length);
    buf[length - 1] = '\0';
    return List_split_strtok(delim, buf);
}

List
List_split_strtok(char * delim, char * str)
{
    List     list   = List_new();
    unsigned length = strlen(delim);
    char *   split;
    
    while ((split = strstr(str, delim)) != NULL) {
        List_push(list, str);
        *split = '\0';
        str    = split + length;
    }

    List_push(list, str);
    return list;
}

List
List_vnew(unsigned number, List_type elem, ...)
{
    List     list  = List_new();
    unsigned index = 0;
    va_list  args;

    List_expand(list, number);
    va_start(args, elem);

    while (number-- > 0) {
        list->array[index++] = elem;
        elem = va_arg(args, List_type);
    }

    va_end(args);
    list->length = index;
    return list;
}

unsigned
List_vpush(List list, unsigned number, List_type elem, ...)
{
    unsigned index = list->offset + list->length;
    va_list  args;

    if (index + number > list->size) {
        List_expand(list, list->size - index + number);
    }
    
    va_start(args, elem);

    while (number-- > 0) {
        list->array[index++] = elem;
        elem = va_arg(args, void *);
    }

    va_end(args);
    return (list->length = index - list->offset);
}
