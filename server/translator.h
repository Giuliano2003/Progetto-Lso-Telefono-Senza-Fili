#ifndef TRANSLATOR_H
#define TRANSLATOR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

typedef struct {
    CURL *curl;
    struct curl_slist *headers;
    char url[256];
} Translator;

void translator_init(Translator *t, const char *url);

int translate(Translator *t, const char *text, const char *source, const char *target, char *out, size_t out_size);

#endif