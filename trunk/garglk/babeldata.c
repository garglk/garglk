/******************************************************************************
 *                                                                            *
 * Copyright (C) 2011 by Ben Cressey.                                         *
 *                                                                            *
 * This file is part of Gargoyle.                                             *
 *                                                                            *
 * Gargoyle is free software; you can redistribute it and/or modify           *
 * it under the terms of the GNU General Public License as published by       *
 * the Free Software Foundation; either version 2 of the License, or          *
 * (at your option) any later version.                                        *
 *                                                                            *
 * Gargoyle is distributed in the hope that it will be useful,                *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with Gargoyle; if not, write to the Free Software                    *
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA *
 *                                                                            *
 *****************************************************************************/

#ifdef BABEL_HANDLER

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glk.h"
#include "garglk.h"

#include "treaty.h"
#include "babel_handler.h"
#include "ifiction.h"

void gli_initialize_babel(void)
{
    if (!strlen(gli_workfile))
        return;

    void *ctx = get_babel_ctx();
    if (babel_init_ctx(gli_workfile, ctx))
    {
        int metaSize = babel_treaty_ctx(GET_STORY_FILE_METADATA_EXTENT_SEL, NULL, 0, ctx);
        if (metaSize > 0)
        {
            char *metaData = malloc(metaSize);
            if (metaData)
            {
                if (babel_treaty_ctx(GET_STORY_FILE_METADATA_SEL, metaData, metaSize, ctx) > 0)
                {
                    char *storyTitle = ifiction_get_tag(metaData, "bibliographic", "title", NULL);
                    char *storyAuthor = ifiction_get_tag(metaData, "bibliographic", "author", NULL);
                    if (storyTitle && storyAuthor)
                    {
                        char title[256];
                        snprintf(title, 255, "%s - %s", storyTitle, storyAuthor);
                        garglk_set_story_title(title);
                        free(storyTitle);
                        free(storyAuthor);
                    }
                }
                free(metaData);
            }
        }
    }
    release_babel_ctx(ctx);
}

#else

void gli_initialize_babel(void)
{
}

#endif /* BABEL_HANDLER */
