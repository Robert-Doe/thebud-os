#ifndef VALUE_H
#define VALUE_H

#include <stddef.h>

/* Tagged union representing a JavaScript value.
   The type tag MUST be checked before accessing the union.
   Failing to do so is a type confusion vulnerability. */

typedef enum {
    VAL_NUMBER,
    VAL_STRING,
    VAL_OBJECT,
    VAL_UNDEFINED
} val_type_t;

typedef struct {
    val_type_t type;
    union {
        double  number;
        char   *string;
        void   *object;
    } u;
} js_value_t;

/* Constructors */
js_value_t val_number(double n);
js_value_t val_string(const char *s);
js_value_t val_object(void *obj);
js_value_t val_undefined(void);

/* Safe accessors — abort if wrong type */
double      val_to_number(js_value_t v);
const char *val_to_string(js_value_t v);
void       *val_to_object(js_value_t v);

/* Type name for display */
const char *val_type_name(val_type_t t);

/* Print value to stdout */
void val_print(js_value_t v);

#endif /* VALUE_H */
