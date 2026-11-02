//
//  vector_common.h
//  scott
//
//  Created by Administrator on 2026-02-01.
//

#ifndef vector_common_h
#define vector_common_h

#include <stddef.h>
#include <stdint.h>

void DrawSomeVectorPixels(int from_start);
int DrawingVector(void);
int TimerDelay(void);
int RunVectorTests(const char *supportpath);
uint8_t *ReadTestDataFromFile(const char *filename, const char *supportpath, size_t *size);

struct USImage;
/* Reads the test image <prefix><name>.dat from supportpath. Returns NULL on
   failure. Free the result with FreeTestImage(). */
struct USImage *LoadTestImage(const char *prefix, const char *name, const char *supportpath);
void FreeTestImage(struct USImage *image);
/* Compares actual with the contents of the test file <prefix><name><suffix>
   in supportpath. Returns 1 if they match. */
int CompareWithTestFile(const char *prefix, const char *name, const char *suffix, const char *supportpath, const uint8_t *actual, size_t actuallen);

typedef enum {
    NO_VECTOR_IMAGE,
    DRAWING_VECTOR_IMAGE,
    SHOWING_VECTOR_IMAGE
} VectorStateType;

extern VectorStateType VectorState;
extern int vector_image_shown;

#endif /* vector_common_h */
