/* Canned protocol compatibility vectors, checked against upstream. */
#pragma once

typedef struct {
    const char *uri;
    const char *expect;
} ref_zip_case_t;

static const ref_zip_case_t ref_zip_cases[] = {
    {"/", ""},
    {"/index.html", "index.html"},
    {"/app/index.html", "index.html"},
    {"/app/0.5.1/relapse/index.html?autoload=payload.elf#frag", "0.5.1/relapse/index.html"},
    {"/document/en/ps5/index.html", "index.html"},
    {"/document/en/ps5/slopkit/slopkit/poops.html?go=1&auto=1", "slopkit/slopkit/poops.html"},
    {"/a/../b", "b"},
    {"/a/./b", "a/b"},
    {"/../../etc/passwd", "etc/passwd"},
    {"/../x", "x"},
    {"/a//b/", "a/b"},
    {"/%2e%2e/etc/passwd", "etc/passwd"},
    {"/a%20b/c", "a b/c"},
    {"/app/", ""},
    {"/app", "app"},
    {"/document/en/ps5/", ""},
    {"/selected_exploit", "selected_exploit"},
    {"/selected_exploit?v=1", "selected_exploit"},
    {"/selected_exploitX", "selected_exploitX"},
    {"//double/slash", "double/slash"},
    {"/a/.../b", "a/.../b"},
    {"/a/.hidden/b", "a/.hidden/b"},
    {"/UPPER/Case.JS", "UPPER/Case.JS"},
    {"/payloads/payload.elf", "payloads/payload.elf"},
    {"/relapse/src/main.js", "relapse/src/main.js"},
    {"/slopkit/slopkit/poops.html", "slopkit/slopkit/poops.html"},
    {"/umtx2/index.html", "umtx2/index.html"},
    {"/shared/elfldr-ps5.elf", "shared/elfldr-ps5.elf"},
    {"/a/b/../../../c", "c"},
    {"/%2e%2e/%2e%2e/etc/passwd", "etc/passwd"},
    {"/a%2Fb/c", "a/b/c"},
    {"/%252e%252e/x", "%2e%2e/x"},
};
#define ref_zip_case_count 32
