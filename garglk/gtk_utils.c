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

#include "glk.h"
#include "garglk.h"
#include "gtk_utils.h"
#include <assert.h>
#include <strings.h>
#include <stdbool.h>
#include <stdlib.h>
#include <wchar.h>
#include <stdio.h>
#include <unistd.h>

#ifdef _KINDLE
#include <openlipc.h>
#endif /* _KINDLE */

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

#ifdef _KINDLE
GString * createAndInitFilenameFromOsEnvironmentVariable(
        const char * environmentVariableName1, 
        const char * environmentVariableName2)
{
    GString * createdString = NULL;
    
    if (getenv(environmentVariableName1)) 
    {
        createdString = g_string_new(getenv(environmentVariableName1));
    }
    else if (getenv(environmentVariableName2))
    {
        createdString = g_string_new(getenv(environmentVariableName2));
    }
    else {
        createdString = g_string_new(NULL);
    }
    normalizeFilename(createdString);
    
    //g_file_test(selectedFilename, G_FILE_TEST_IS_DIR);
    
    return createdString;
}

static LIPC * lipcInstance = 0;

void openLipcInstance() {
	if (lipcInstance == 0) {
		fwprintf(stderr, L"---->openLipcInstance()\n");
		lipcInstance = LipcOpen("net.fabiszewski.gargoyle");
	}
}

void closeLipcInstance() {
	if (lipcInstance != 0) {
		fwprintf(stderr, L"---->closeLipcInstance()\n");
		LipcClose(lipcInstance);
	}
}

void openVirtualKeyboard(GtkWidget * widget, gpointer * callback_data) {
    /* lipc-set-prop -s com.lab126.keyboard open net.fabiszewski.gargoyle:abc:0 */
    if (lipcInstance == 0) {
		openLipcInstance();
	}
	
	int isKeboardVisible = 0;
	LipcGetIntProperty(
                lipcInstance,
                "com.lab126.keyboard",
                "show", &isKeboardVisible);
	
	if (isKeboardVisible == 0) {
		fwprintf(stderr, L"---->openVirtualKeyboard\n");
    	LipcSetStringProperty(lipcInstance,
                          "com.lab126.keyboard",
                          "open",
                          "net.fabiszewski.gargoyle:abc:0");
    } 
    else {
    	fwprintf(stderr, L"---->openVirtualKeyboard - Keyboard already opened, doing noting.\n");
    }
    /* int pid = fork();
	if (pid == 0) {
		char *args[] = { "/usr/bin/lipc-set-prop",
                    "-s",
                    "com.lab126.keyboard",
                    "open",
                    "net.fabiszewski.gargoyle:abc:0",
                    NULL };
        fwprintf(stderr, L"---->openVirtualKeyboard-execv()\n");
		execv(args[0], args); 
		exit(EXIT_SUCCESS);
	}
	*/
}

GtkWidget * createAndInitKindleFileRequestor(
        const GString * initFilename,
        const GtkSortType directoryListSortOrder,
        const GtkSortType filenameListSortOrder)
{
    GdkScreen * screen = gdk_screen_get_default();
    gint screen_height = gdk_screen_get_height(screen);
    gint screen_width = gdk_screen_get_width(screen);

    GtkFileSelection * fileRequestor = GTK_FILE_SELECTION(gtk_file_selection_new(KDIALOG));
    gtk_signal_connect(GTK_OBJECT(fileRequestor), "focus_in_event",
                       GTK_SIGNAL_FUNC(openVirtualKeyboard), NULL);
    //fwprintf(stderr, L"---->Signal connected\n");
    
    //gtk_widget_hide(GTK_FILE_SELECTION(filedlog)->history_pulldown);
    // K*ndle GTK port does not properly support/fully implement fileop buttons in GtkFileSelection.
    gtk_file_selection_hide_fileop_buttons(fileRequestor);
    gtk_widget_set_size_request(GTK_WIDGET(fileRequestor), screen_width, (screen_height - screen_height/KBFACTOR));
    gtk_window_set_resizable(GTK_WINDOW(fileRequestor), FALSE);

    // Make the filename list sortable and sort it in descending order by default.
    makeFilenameListTreeViewSortable(
            GTK_TREE_VIEW(fileRequestor->file_list), 
            filenameListSortOrder);

    // Make the directory-name list sortable and sort it in ascending order by default.
    makeFilenameListTreeViewSortable(
            GTK_TREE_VIEW(fileRequestor->dir_list), 
            directoryListSortOrder);

    gtk_file_selection_set_filename(fileRequestor, initFilename->str);
    
    return GTK_WIDGET(fileRequestor);
}
#endif /* _KINDLE */