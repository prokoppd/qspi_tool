#ifndef INTERNALS_H
#define INTERNALS_H

#ifdef TESTING
#define STATIC
#else
#define STATIC static
#endif

#define lengthof(x) (sizeof(x) / sizeof((x)[0]))

#endif // INTERNALS_H
