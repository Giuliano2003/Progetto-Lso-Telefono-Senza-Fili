#include "translator.h"

struct string {
    char* ptr;
    size_t len;
};

void init_string(struct string* s) {
    s->len = 0;
    s->ptr = malloc(1);
    if (s->ptr) s->ptr[0] = '\0';
}

size_t writefunc(void* ptr, size_t size, size_t nmemb, struct string* s) {
    size_t new_len = s->len + size * nmemb;
    s->ptr = realloc(s->ptr, new_len + 1);
    if (s->ptr == NULL) {
        fprintf(stderr, "realloc() failed\n");
        exit(EXIT_FAILURE);
    }
    memcpy(s->ptr + s->len, ptr, size * nmemb);
    s->ptr[new_len] = '\0';
    s->len = new_len;
    return size * nmemb;
}

void translator_init(Translator* t, const char* url) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    t->curl = curl_easy_init();
    t->headers = NULL;
    t->headers = curl_slist_append(t->headers, "Content-Type: application/x-www-form-urlencoded");
    strncpy(t->url, url, sizeof(t->url)-1);
    t->url[sizeof(t->url)-1] = '\0';
}

int translate(Translator *t, const char *text, const char *source, const char *target, char *out, size_t out_size) {
    if (!t->curl) return 1;
    struct string response;
    char postfields[1024];
    CURLcode res;

    snprintf(postfields, sizeof(postfields),
        "q=%s&source=%s&target=%s&format=text",
        text, source, target);

    init_string(&response);

    curl_easy_setopt(t->curl, CURLOPT_URL, t->url);
    curl_easy_setopt(t->curl, CURLOPT_HTTPHEADER, t->headers);
    curl_easy_setopt(t->curl, CURLOPT_POST, 1L);
    curl_easy_setopt(t->curl, CURLOPT_POSTFIELDS, postfields);
    curl_easy_setopt(t->curl, CURLOPT_WRITEFUNCTION, writefunc);
    curl_easy_setopt(t->curl, CURLOPT_WRITEDATA, &response);

    res = curl_easy_perform(t->curl);
    if(res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        free(response.ptr);
        return 1;
    } else {
        const char *key = "\"translatedText\":\"";
        char *start = strstr(response.ptr, key);
        if (start) {
            start += strlen(key);
            char *end = strchr(start, '"');
            size_t len = end ? (size_t)(end - start) : strlen(start);
            if (len >= out_size) len = out_size - 1;
            memcpy(out, start, len);
            out[len] = '\0';
        } else {
            size_t len = strlen(response.ptr);
            if (len >= out_size) len = out_size - 1;
            memcpy(out, response.ptr, len);
            out[len] = '\0';
        }
    }
    free(response.ptr);
    return 0;
}