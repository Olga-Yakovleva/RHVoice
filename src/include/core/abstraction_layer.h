#pragma once
#ifndef RHVOICE_ABSTR_LAYER
#define RHVOICE_ABSTR_LAYER

#if defined(_WIN32) AND defined(UNICODE) AND UNICODE
    typedef wchar_t PathCharT;
#else
    typedef char PathCharT;

    #define TEXT(t) t
#endif

PathCharT* ourPathGetenv(char* varName);

#if defined(__cplusplus)
    #include <string>
    typedef std::basic_string<PathCharT> PathT;
    typedef std::basic_ifstream<char> IStreamT;
    typedef std::basic_ofstream<char> OStreamT;
    std::string getShortPathIfNeeded(PathCharT *wide);
#endif

#endif
