//
//  vector_common.c
//  scott
//
//  Created by Administrator on 2026-02-01.
//

#include <stdlib.h>
#include <string.h>

#include "apple2_vector_draw.h"
#include "atari_8bit_vector_draw.h"
#include "common_file_utils.h"
#include "line_drawing.h"
#include "sagagraphics.h"
#include "scott.h"
#include "vector_common.h"
#include "vector_oplist.h"

/* Timer intervals (ms) for slow drawing. Two US pictures are slower. */
#define US_VECTOR_DELAY 10
#define SLOW_VECTOR_DELAY 30
#define HOWARTH_VECTOR_DELAY 20

VectorStateType VectorState = NO_VECTOR_IMAGE;

void DrawSomeVectorPixels(int from_start) {
    if (CurrentSys == SYS_APPLE2) {
        DrawSomeApple2VectorBytes(from_start);
    } else if (CurrentSys == SYS_ATARI8) {
        DrawSomeAtari8VectorBytes(from_start);
    } else {
        DrawSomeHowarthVectorPixels(from_start);
    }
}

int DrawingVector(void) {
    if (CurrentSys == SYS_APPLE2) {
        return DrawingApple2Vector();
    } else if (CurrentSys == SYS_ATARI8) {
        return DrawingAtari8Vector();
    }  else {
        return DrawingHowarthVector();
    }
}

int TimerDelay(void) {
    if (CurrentSys == SYS_APPLE2 || CurrentSys == SYS_ATARI8) {
        if ((CurrentGame == PIRATE_US && vector_image_shown == 80) ||
            (CurrentGame == SECRET_MISSION_US && vector_image_shown == 88))
            return SLOW_VECTOR_DELAY;
        return US_VECTOR_DELAY;
    }  else {
        return HOWARTH_VECTOR_DELAY;
    }
}

static void StartImageSession(VectorOpList *list) {
    VectorOpListStartSession(list);
    glk_request_timer_events(0);
    VectorState = DRAWING_VECTOR_IMAGE;
}

int VectorBeginImage(VectorOpList *list, const USImage *img, void (*redraw_shown_image)(void)) {
    if (img->usage == IMG_ROOM) {
        // Room images reset any drawing
        StartImageSession(list);
        vector_image_shown = img->index;
    } else if (VectorState == SHOWING_VECTOR_IMAGE) {
        // If this is not a room image and we are already showing an image,
        // we can assume that we want to draw a room object on top of
        // the current room image
        redraw_shown_image();
        StartImageSession(list);
    } else if (list->ops == NULL) {
        return 0;
    }
    return 1;
}

uint8_t *ReadTestDataFromFile(const char *filename, const char *supportpath, size_t *size) {
    size_t pathlength = strlen(supportpath) + strlen(filename) + 1;
    char *pathname = MemAlloc(pathlength);
    snprintf(pathname, pathlength, "%s%s", supportpath, filename);
    uint8_t *result = ReadFileIfExists(pathname, size);
    if (!result)
        fprintf(stderr, "ReadTestDataFromFile: Failed to read file at \"%s\"\n", pathname);
    free(pathname);
    return result;
}

static char *TestFileName(const char *prefix, const char *name, const char *suffix) {
    size_t length = strlen(prefix) + strlen(name) + strlen(suffix) + 1;
    char *filename = MemAlloc(length);
    snprintf(filename, length, "%s%s%s", prefix, name, suffix);
    return filename;
}

USImage *LoadTestImage(const char *prefix, const char *name, const char *supportpath) {
    char *filename = TestFileName(prefix, name, ".dat");
    size_t size;
    uint8_t *data = ReadTestDataFromFile(filename, supportpath, &size);
    free(filename);
    if (!data) {
        fprintf(stderr, "Failed to read image data\n");
        return NULL;
    }
    USImage *image = NewImage();
    image->imagedata = data;
    image->datasize = size;
    return image;
}

void FreeTestImage(USImage *image) {
    free(image->imagedata);
    free(image);
}

int CompareWithTestFile(const char *prefix, const char *name, const char *suffix, const char *supportpath, const uint8_t *actual, size_t actuallen) {
    char *filename = TestFileName(prefix, name, suffix);
    fprintf(stderr, "CompareWithTestFile: Comparison with file %s\n", filename);
    size_t size;
    uint8_t *expected = ReadTestDataFromFile(filename, supportpath, &size);
    free(filename);
    if (expected == NULL || size == 0) {
        fprintf(stderr, "Bad file!\n");
        free(expected);
        return 0;
    }
    if (size != actuallen)
        fprintf(stderr, "Size mismatch: file has 0x%zx bytes, screen memory 0x%zx. Comparing the first 0x%zx\n", size, actuallen, MIN(size, actuallen));
    size = MIN(size, actuallen);
    int result = 1;
    for (size_t i = 0; i < size; i++) {
        if (expected[i] != actual[i]) {
            fprintf(stderr, "Mismatch at 0x%04zx: expected 0x%02x, got 0x%02x\n", i, expected[i], actual[i]);
            result = 0;
            break;
        }
    }
    free(expected);
    return result;
}

int RunVectorTests(const char *supportpath) {
    if (!RunApple2VectorTests(supportpath))
        return 0;
    return RunAtari8bitVectorTests(supportpath);
}
