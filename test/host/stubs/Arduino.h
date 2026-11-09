#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
static inline void* ps_malloc(size_t n){return malloc(n);}
struct SerialStub{ template<class...A> void printf(const char*f,A...a){::printf(f,a...);} void println(const char*s){puts(s);} void print(const char*s){fputs(s,stdout);} };
static SerialStub Serial;
uint32_t millis(); uint32_t micros();
class String: public std::string { public: String(){} String(const char*s):std::string(s?s:""){} };
