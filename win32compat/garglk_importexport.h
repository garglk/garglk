
#ifndef GARGLK_IMPORTEXPORT_H
#define GARGLK_IMPORTEXPORT_H

#ifdef _MSC_VER

#ifdef garglk_EXPORTS
#define GARGLK_API __declspec(dllexport)
#else
#define GARGLK_API __declspec(dllimport)
#endif

#else
#define GARGLK_API
#endif

#endif