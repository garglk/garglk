/* Code from Exomizer distributed under the zlib License
 * by kind permission of the original author
 * Magnus Lind.
 */

/*
 * Copyright (c) 2008 - 2023 Magnus Lind.
 *
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 * claim that you wrote the original software. If you use this software
 * in a product, an acknowledgment in the product documentation would be
 * appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "exo_util.h"

int find_sys(const unsigned char *buf, int target)
{
    int outstart = -1;
    int state = 1;
    int i = 0;
    /* skip link and line number */
    buf += 4;
    /* iAN: workaround for hidden sysline (1001 cruncher, CFB, etc)*/
    if (buf[0] == 0) {
        for (i = 5; i < 32; i++)
            if (buf[i] == 0x9e && (((buf[i + 1] & 0x30) == 0x30) || ((buf[i + 2] & 0x30) == 0x30)))
                break;
    }
    /* exit loop at line end */
    while (i < 1000 && buf[i] != '\0') {
        unsigned char *sys_end;
        int c = buf[i];
        switch (state) {
            /* look for and consume sys token */
        case 1:
            if ((target == -1 && (c == 0x9e /* cbm */ /* ||
                             c == 0x8c*/
                     /* apple 2*/ /* ||
                        c == 0xbf*/
                     /* oric 1*/))
                || c == target) {
                state = 2;
            }
            break;
            /* skip spaces and left parenthesis, if any */
        case 2:
            if (strchr(" (", c) != NULL)
                break;
            /* convert string number to int */
        case 3:
            outstart = (int)strtol((const char *)(buf + i), (void *)&sys_end, 10);
            if ((buf + i) == sys_end) {
                /* we got nothing */
                outstart = -1;
            } else {
                c = *sys_end;
                if ((c >= 0xaa) && (c <= 0xae)) {
                    i = (int)strtol((char *)(sys_end + 1), (void *)&sys_end, 10);
                    switch (c) {
                    case 0xaa:
                        outstart += i;
                        break;
                    case 0xab:
                        outstart -= i;
                        break;
                    case 0xac:
                        outstart *= i;
                        break;
                    case 0xad:
                        if (i > 0)
                            outstart /= i;
                        break;
                    case 0xae:
                        c = outstart;
                        while (--i)
                            outstart *= c;
                        break;
                    }
                } else if (c == 'E') {
                    i = (int)strtol((char *)(sys_end + 1), (void *)&sys_end, 10);
                    i++;
                    while (--i)
                        outstart *= 10;
                }
            }
            state = 4;
            break;
        case 4:
            break;
        }
        ++i;
    }

    LOG(LOG_DEBUG, ("state when leaving: %d.\n", state));
    return outstart;
}

static int get_byte(FILE *in)
{
    int byte = fgetc(in);
    if (byte == EOF) {
        // LOG(LOG_ERROR, ("Error: unexpected end of xex-file."));
        fclose(in);
        exit(-1);
    }
    return byte;
}

static int get_le_word(FILE *in)
{
    int word = get_byte(in);
    word |= get_byte(in) << 8;
    return word;
}
//#if 0
// static int get_be_word(FILE *in)
//{
//    int word = get_byte(in) << 8;
//    word |= get_byte(in);
//    return word;
//}
//#endif

static FILE *open_file(char *name, int *load_addr)
{
    FILE *in;
    int is_plain = 0;
    int is_relocated = 0;
    int load = -3;

    do {
        char *load_str;
        char *at_str;

        in = fopen(name, "rb");
        if (in != NULL) {
            /* We have succeded in opening the file.
       * There's no address suffix. */
            break;
        }

        /* hmm, let's see if the user is trying to relocate it */
        load_str = strrchr(name, ',');
        at_str = strrchr(name, '@');
        if (at_str != NULL && (load_str == NULL || at_str > load_str)) {
            is_plain = 1;
            load_str = at_str;
        }

        if (load_str == NULL) {
            /* nope, */
            break;
        }

        *load_str = '\0';
        ++load_str;
        is_relocated = 1;

        /* relocation was requested */
        if (str_to_int(load_str, &load) != 0) {
            /* we fail */
            LOG(LOG_FATAL, (" can't parse load address from \"%s\"\n", load_str));
            exit(-1);
        }

        in = fopen(name, "rb");

    } while (0);
    if (in == NULL) {
        LOG(LOG_FATAL, (" can't open file \"%s\" for input\n", name));
        exit(-1);
    }

    if (!is_plain) {
        /* read the prg load address */
        int prg_load = get_le_word(in);
        if (!is_relocated) {
            load = prg_load;
            //#if 0
            //            /* unrelocated prg loading to $ffff is xex */
            //            if(prg_load == 0xffff)
            //            {
            //                /* differentiate this from relocated $ffff files so it
            //                is
            //                 * possible to override the xex auto-detection. */
            //                load = -1;
            //            }
            //            /* unrelocated prg loading to $1616 is Oric tap */
            //            else if(prg_load == 0x1616)
            //            {
            //                load = -2;
            //            }
            //#endif
        }
    }

    if (load_addr != NULL) {
        *load_addr = load;
    }
    return in;
}

//#if 0
// static void load_xex(unsigned char mem[65536], FILE *in,
//                     struct load_info *info)
//{
//    int run = -1;
//    int jsr = -1;
//    int min = 65536, max = 0;
//
//    goto initial_state;
//    for(;;)
//    {
//        int start, end, len;
//
//        start = fgetc(in);
//        if(start == EOF) break;
//        ungetc(start, in);
//
//        start = get_le_word(in);
//        if(start == 0xffff)
//        {
//            /* allowed optional header */
//        initial_state:
//            start = get_le_word(in);
//        }
//        end = get_le_word(in);
//        if(start > 0xffff || end > 0xffff || end < start)
//        {
//            LOG(LOG_ERROR, ("Error: corrupt data in xex-file."));
//            fclose(in);
//            exit(-1);
//        }
//        if(start == 0x2e2 && end == 0x2e3)
//        {
//            /* init vector */
//            jsr = get_le_word(in);
//            LOG(LOG_VERBOSE, ("Found xex initad $%04X.\n", jsr));
//            continue;
//        }
//        if(start == 0x2e0 && end == 0x2e1)
//        {
//            /* run vector */
//            run = get_le_word(in);
//            LOG(LOG_VERBOSE, ("Found xex runad $%04X.\n", run));
//            continue;
//        }
//        ++end;
//        jsr = -1;
//        if(start < min) min = start;
//        if(end > max) max = end;
//
//        len = fread(mem + start, 1, end - start, in);
//        if(len != end - start)
//        {
//            LOG(LOG_ERROR, ("Error: unexpected end of xex-file.\n"));
//            fclose(in);
//            exit(-1);
//        }
//        LOG(LOG_VERBOSE, (" xex chunk loading from $%04X to $%04X\n",
//                          start, end));
//    }
//
//    if(run == -1 && jsr != -1) run = jsr;
//
//    info->start = min;
//    info->end = max;
//    info->basic_var_start = -1;
//    info->run = -1;
//    if(run != -1)
//    {
//        info->run = run;
//    }
//}
//
// static void load_oric_tap(unsigned char mem[65536], FILE *in,
//                          struct load_info *info)
//{
//    int c;
//    int autostart;
//    int start, end, len;
//
//    /* read oric tap header */
//
//    /* next byte must be 0x16 as we have already read two and must
//     * have at least three */
//    if(get_byte(in) != 0x16)
//    {
//        LOG(LOG_ERROR, ("Error: fewer than three lead-in bytes ($16) "
//                        "in Oric tap-file header.\n"));
//        fclose(in);
//        exit(-1);
//    }
//    /* optionally more 0x16 bytes */
//    while((c = get_byte(in)) == 0x16);
//    /* next byte must be 0x24 */
//    if(c != 0x24)
//    {
//        LOG(LOG_ERROR, ("Error: bad sync byte after lead-in in Oric tap-file "
//                        "header, got $%02X but expected $24\n", c));
//        fclose(in);
//        exit(-1);
//    }
//
//    /* now we are in sync, lets be lenient */
//    get_byte(in); /* should be 0x0 */
//    get_byte(in); /* should be 0x0 */
//    get_byte(in); /* should be 0x0 or 0x80 */
//    autostart = (get_byte(in) != 0);  /* should be 0x0, 0x80 or 0xc7 */
//    end = get_be_word(in) + 1; /* the header end address is inclusive */
//    start = get_be_word(in);
//    get_byte(in); /* should be 0x0 */
//    /* read optional file name */
//    while(get_byte(in) != 0x0);
//
//    /* read the data */
//    len = fread(mem + start, 1, end - start, in);
//    if(len != end - start)
//    {
//        LOG(LOG_BRIEF, ("Warning: Oric tap-file contains %d byte(s) data "
//                        "less than expected.\n", end - start - len));
//        end = start + len;
//    }
//    LOG(LOG_VERBOSE, (" Oric tap-file loading from $%04X to $%04X\n",
//                      start, end));
//
//    /* fill in the fields */
//    info->start = start;
//    info->end = end;
//    info->run = -1;
//    info->basic_var_start = -1;
//    if(autostart)
//    {
//        info->run = start;
//    }
//    if(info->basic_txt_start >= start &&
//       info->basic_txt_start < end)
//    {
//        info->basic_var_start = end - 1;
//    }
//}
//#endif
static void load_prg(unsigned char mem[65536], FILE *in,
    struct load_info *info)
{
    int len;
    len = (int)fread(mem + info->start, 1, 65536 - info->start, in);

    info->end = info->start + len;
    info->basic_var_start = -1;
    info->run = -1;
    if (info->basic_txt_start >= info->start && info->basic_txt_start < info->end) {
        info->basic_var_start = info->end;
    }
}

#define MIN(a, b) ((a) < (b) ? (a) : (b))

static void load_prg_data(unsigned char mem[65536], uint8_t *data,
    size_t data_length, struct load_info *info)
{
    int len = MIN(65536 - info->start, (int)data_length);
    memcpy(mem + info->start, data, len);

    info->end = info->start + len;
    info->basic_var_start = -1;
    info->run = -1;
    if (info->basic_txt_start >= info->start && info->basic_txt_start < info->end) {
        info->basic_var_start = info->end;
    }
}

void load_located(char *filename, unsigned char mem[65536],
    struct load_info *info)
{
    int load;
    FILE *in;

    in = open_file(filename, &load);
#if 0
    if(load == -1)
    {
        /* file is an xex file */
        load_xex(mem, in, info);
    }
    else if(load == -2)
    {
        /* file is an oric tap file */
        load_oric_tap(mem, in, info);
    }
    else
#endif
    {
        /* file is a located plain file or a prg file */
        info->start = load;
        load_prg(mem, in, info);
    }
    fclose(in);

    LOG(LOG_NORMAL, (" filename: \"%s\", loading from $%04X to $%04X\n", filename, info->start, info->end));
}

void load_data(uint8_t *data, size_t data_length, unsigned char mem[65536],
    struct load_info *info)
{
    int load = data[0] + data[1] * 0x100;

    info->start = load;
    load_prg_data(mem, data + 2, data_length - 2, info);

    //    LOG(LOG_NORMAL,
    //        ("loading data from $%04X to $%04X\n",
    //         info->start, info->end));
}

/* returns 0 if ok, 1 otherwise */
int str_to_int(const char *str, int *value)
{
    int status = 0;
    do {
        char *str_end;
        long lval;

        /* base 0 is auto detect */
        int base = 0;

        if (*str == '\0') {
            /* no string to parse */
            status = 1;
            break;
        }

        if (*str == '$') {
            /* a $ prefix specifies base 16 */
            ++str;
            base = 16;
        }

        lval = strtol(str, &str_end, base);

        if (*str_end != '\0') {
            /* there is garbage in the string */
            status = 1;
            break;
        }

        if (value != NULL) {
            /* all is well, set the out parameter */
            *value = (int)lval;
        }
    } while (0);

    return status;
}

const char *fixup_appl(char *appl)
{
    char *applp;

    /* strip pathprefix from appl */
    applp = strrchr(appl, '\\');
    if (applp != NULL) {
        appl = applp + 1;
    }
    applp = strrchr(appl, '/');
    if (applp != NULL) {
        appl = applp + 1;
    }
    /* strip possible exe suffix */
    applp = appl + strlen(appl) - 4;
    if (strcmp(applp, ".exe") == 0 || strcmp(applp, ".EXE") == 0) {
        *applp = '\0';
    }
    return appl;
}
