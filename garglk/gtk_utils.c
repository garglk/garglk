/******************************************************************************
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

/* 
 * File:   gtk_utils.c
 * Author: schoen
 * 
 * Created on September 17, 2017, 8:34 PM
 */

#include "gtk_utils.h"
#include <strings.h>

GString * normalizeFilename(GString * filename) 
{
    if (g_file_test(filename->str, G_FILE_TEST_IS_DIR) 
            && filename->len > 0 
            && !(filename->len == 1 && filename->str[0] == '/'))
    {
        g_string_append_c(filename, '/');
    }
}

gint filenameListSortFunc(GtkTreeModel * model,
        GtkTreeIter * a, GtkTreeIter * b, gpointer user_data)
{
    gchar * filenamA;
    gchar * filenamB;
    gtk_tree_model_get(model, a, 0, &filenamA, -1);
    gtk_tree_model_get(model, b, 0, &filenamB, -1);
    
    int result = strcasecmp(filenamA, filenamB);
    
    g_free(filenamA);
    g_free(filenamB);
    return result;
}

void makeFilenameListTreeViewSortable(GtkTreeView * filenameListTreeView, GtkSortType initialSortOrder) 
{   
    GtkTreeViewColumn *fileNameColumn = gtk_tree_view_get_column(filenameListTreeView, 0);
    gtk_tree_view_column_set_sort_indicator(fileNameColumn, TRUE);
    
    gtk_tree_sortable_set_sort_func(
            GTK_TREE_SORTABLE(gtk_tree_view_get_model(filenameListTreeView)), 
            0, filenameListSortFunc, NULL, NULL);
    gtk_tree_sortable_set_sort_column_id(
            GTK_TREE_SORTABLE(gtk_tree_view_get_model(filenameListTreeView)), 
            0, initialSortOrder);
    
    gtk_tree_view_column_set_sort_column_id(fileNameColumn, 0);
}
