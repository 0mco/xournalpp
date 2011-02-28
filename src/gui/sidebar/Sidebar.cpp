#include "Sidebar.h"
#include "../../model/LinkDestination.h"
#include <string.h>

#include <config.h>
#include <glib/gi18n-lib.h>

Sidebar::Sidebar(GladeGui * gui, Control * control) {
	this->control = control;
	this->treeViewBookmarks = gtk_tree_view_new();
	this->iconViewPreview = gtk_layout_new(NULL, NULL);
	this->typeSelected = false;
	this->selectedPage = -1;
	this->cache = new PdfCache(control->getSettings()->getPdfPageCacheSize());

	this->buttonCloseSidebar = gui->get("buttonCloseSidebar");

	this->zoom = 0.15;

	this->backgroundInitialized = false;

	this->previews = NULL;
	this->previewCount = 0;

	g_object_ref(this->treeViewBookmarks);
	g_object_ref(this->iconViewPreview);
	GtkWidget * sidebar = gui->get("sidebarContents");

	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(treeViewBookmarks), true);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(treeViewBookmarks), DOCUMENT_LINKS_COLUMN_NAME);
	gtk_tree_view_set_search_equal_func(GTK_TREE_VIEW(treeViewBookmarks),
			(GtkTreeViewSearchEqualFunc) treeSearchFunction, this, NULL);

	GtkTreeViewColumn * column;
	GtkCellRenderer * renderer;
	GtkTreeSelection * selection;

	scrollBookmarks = gtk_scrolled_window_new(NULL, NULL);

	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollBookmarks), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrollBookmarks), GTK_SHADOW_IN);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW (treeViewBookmarks));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW (treeViewBookmarks), FALSE);
	gtk_container_add(GTK_CONTAINER (scrollBookmarks), treeViewBookmarks);

	gtk_box_pack_start(GTK_BOX (sidebar), scrollBookmarks, TRUE, TRUE, 0);

	gtk_widget_show_all(GTK_WIDGET (sidebar));
	gtk_widget_set_visible(GTK_WIDGET (sidebar), control->getSettings()->isSidebarVisible());

	scrollPreview = gtk_scrolled_window_new(NULL, NULL);

	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollPreview), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrollPreview), GTK_SHADOW_IN);
	gtk_container_add(GTK_CONTAINER (scrollPreview), iconViewPreview);
	gtk_box_pack_start(GTK_BOX(sidebar), scrollPreview, TRUE, TRUE, 0);

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_expand(GTK_TREE_VIEW_COLUMN (column), TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW (treeViewBookmarks), column);

	renderer = (GtkCellRenderer*) g_object_new(GTK_TYPE_CELL_RENDERER_TEXT, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	gtk_tree_view_column_pack_start(GTK_TREE_VIEW_COLUMN (column), renderer, TRUE);
	gtk_tree_view_column_set_attributes(GTK_TREE_VIEW_COLUMN (column), renderer, "markup", DOCUMENT_LINKS_COLUMN_NAME,
			NULL);

	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_end(GTK_TREE_VIEW_COLUMN (column), renderer, FALSE);
	gtk_tree_view_column_set_attributes(GTK_TREE_VIEW_COLUMN (column), renderer, "text",
			DOCUMENT_LINKS_COLUMN_PAGE_NUMBER, NULL);
	g_object_set(G_OBJECT (renderer), "style", PANGO_STYLE_ITALIC, NULL);

	GtkCellRenderer *cell;
	GtkListStore *store;

	comboBox = GTK_COMBO_BOX(gui->get("cbSelectSidebar"));

	store = gtk_list_store_new(1, G_TYPE_STRING);
	gtk_combo_box_set_model(comboBox, GTK_TREE_MODEL(store));
	g_object_unref(store);

	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(comboBox), cell, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(comboBox), cell, "text", 0, NULL);

	gtk_combo_box_append_text(comboBox, _("Bookmarks"));
	gtk_combo_box_append_text(comboBox, _("Preview"));
	gtk_combo_box_set_active(comboBox, 0);

	g_signal_connect(treeViewBookmarks, "button_press_event", G_CALLBACK(treeClickedCallback), this);
	g_signal_connect(comboBox, "changed", G_CALLBACK(cbChangedCallback), this);

	registerListener(control);
}

void Sidebar::setBackgroundWhite() {
	if (this->backgroundInitialized) {
		return;
	}
	this->backgroundInitialized = true;
	gdk_window_set_background(GTK_LAYOUT(iconViewPreview)->bin_window, &iconViewPreview->style->white);
}

Sidebar::~Sidebar() {
	gtk_widget_destroy(this->treeViewBookmarks);
	gtk_widget_destroy(this->iconViewPreview);

	delete this->cache;
	this->cache = NULL;
}

gboolean Sidebar::treeSearchFunction(GtkTreeModel *model, gint column, const gchar *key, GtkTreeIter *iter,
		Sidebar * sidebar) {
	// Source: Pidgin

	gchar *enteredstring;
	gchar *tmp;
	gchar *text;
	gchar *normalized;
	gboolean result;
	size_t i;
	size_t len;
	PangoLogAttr *log_attrs;
	gchar *word;

	gtk_tree_model_get(model, iter, DOCUMENT_LINKS_COLUMN_NAME, &text, -1);
	if (text == NULL)
		return TRUE;

	tmp = g_utf8_normalize(key, -1, G_NORMALIZE_DEFAULT);
	enteredstring = g_utf8_casefold(tmp, -1);
	g_free(tmp);

	tmp = g_utf8_normalize(text, -1, G_NORMALIZE_DEFAULT);
	normalized = g_utf8_casefold(tmp, -1);
	g_free(tmp);

	if (g_str_has_prefix(normalized, enteredstring)) {
		g_free(enteredstring);
		g_free(normalized);
		return FALSE;
	}

	/* Use Pango to separate by words. */
	len = g_utf8_strlen(normalized, -1);
	log_attrs = g_new(PangoLogAttr, len + 1);

	pango_get_log_attrs(normalized, strlen(normalized), -1, NULL, log_attrs, len + 1);

	word = normalized;
	result = TRUE;
	for (i = 0; i < (len - 1); i++) {
		if (log_attrs[i].is_word_start && g_str_has_prefix(word, enteredstring)) {
			result = FALSE;
			break;
		}
		word = g_utf8_next_char(word);
	}
	g_free(log_attrs);

	g_free(enteredstring);
	g_free(normalized);

	return result;
}

void Sidebar::cbChangedCallback(GtkComboBox * widget, Sidebar * sidebar) {
	int selected = gtk_combo_box_get_active(widget);

	if (selected == 0) { // Bookmark
		gtk_widget_hide(sidebar->scrollPreview);
		gtk_widget_show_all(sidebar->scrollBookmarks);
	} else if (selected == 1) { // Preview
		gtk_widget_hide(sidebar->scrollBookmarks);
		gtk_widget_show_all(sidebar->scrollPreview);
	}
}

void Sidebar::askInsertPdfPage(int pdfPage) {
	GtkWidget
			* dialog =
					gtk_message_dialog_new(
							(GtkWindow*) *control->getWindow(),
							GTK_DIALOG_DESTROY_WITH_PARENT,
							GTK_MESSAGE_QUESTION,
							GTK_BUTTONS_NONE,
							_("Your current document does not contain PDF Page %i\n"
									"Would you insert this page?\n\nTipp: You can select Journal / Paper Background / PDF Background to insert a PDF page."),
							pdfPage + 1);

	gtk_dialog_add_button(GTK_DIALOG(dialog), "Cancel", 1);
	gtk_dialog_add_button(GTK_DIALOG(dialog), "Insert after", 2);
	gtk_dialog_add_button(GTK_DIALOG(dialog), "Insert at end", 3);

	int res = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	if (res == 1) {
		return;
	}

	int position = 0;
	if (res == 2) {
		position = control->getCurrentPageNo() + 1;
	} else if (res == 3) {
		position = control->getDocument()->getPageCount();
	}

	XojPopplerPage * pdf = control->getDocument()->getPdfPage(pdfPage);
	if (pdf) {
		XojPage * page = new XojPage(pdf->getWidth(), pdf->getHeight());
		page->setBackgroundPdfPageNr(pdfPage);
		control->insertPage(page, position);
	}
}

bool Sidebar::treeClickedCallback(GtkWidget *treeview, GdkEventButton *event, Sidebar *sidebar) {
	GtkTreePath * path = NULL;
	sidebar->typeSelected = true;

	gtk_widget_grab_focus(GTK_WIDGET(treeview));

	if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(treeview), event->x, event->y, &path, NULL, NULL, NULL)) {
		// Not editable
		gtk_tree_view_set_cursor(GTK_TREE_VIEW(treeview), path, NULL, FALSE);
		gtk_tree_path_free(path);

		GtkTreeModel *model = NULL;
		GtkTreeIter iter = { 0 };

		GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));

		if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
			XojLinkDest * link = NULL;
			int first_page, last_page = -1;

			gtk_tree_model_get(model, &iter, DOCUMENT_LINKS_COLUMN_LINK, &link, -1);
			if (link && link->dest) {
				LinkDestination *dest = link->dest;

				int pdfPage = dest->getPdfPage();

				if (pdfPage >= 0) {
					Document * doc = sidebar->control->getDocument();
					int page = doc->findPdfPage(pdfPage);

					if (page == -1) {
						sidebar->askInsertPdfPage(pdfPage);
					} else {
						if (dest->shouldChangeTop()) {
							sidebar->control->scrollToPage(page, dest->getTop());
						} else {
							if (sidebar->control->getCurrentPageNo() != page) {
								sidebar->control->scrollToPage(page);
							}
						}
					}

					// TODO LOW PRIO / OPTIONAL?: Manage scroll to left coordinate and change zoom if dest->shouldChangeZoom()
				}
			}

			return true;
		}
	}
	return false;
}

int Sidebar::expandOpenLinks(GtkTreeModel *model, GtkTreeIter *parent) {
	GtkTreeIter iter = { 0 };
	XojLinkDest * link = NULL;
	if (model == NULL) {
		return 0;
	}

	int count = 0;

	if (gtk_tree_model_iter_children(model, &iter, parent)) {
		do {
			gtk_tree_model_get(model, &iter, DOCUMENT_LINKS_COLUMN_LINK, &link, -1);
			if (link->dest->getExpand()) {
				GtkTreePath * path = gtk_tree_model_get_path(model, &iter);
				gtk_tree_view_expand_row(GTK_TREE_VIEW(treeViewBookmarks), path, FALSE);
				gtk_tree_path_free(path);
			}
			count++;

			count += expandOpenLinks(model, &iter);
		} while (gtk_tree_model_iter_next(model, &iter));
	}

	return count;
}

bool Sidebar::selectPageNr(int page, int pdfPage, GtkTreeIter * parent) {
	GtkTreeIter iter;
	GtkTreeModel * model = control->getDocument()->getContentsModel();
	if (model == NULL) {
		return false;
	}

	if (parent == NULL) {
		// check if there is already the current page selected
		GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeViewBookmarks));
		if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
			XojLinkDest * link = NULL;
			int first_page, last_page = -1;

			gtk_tree_model_get(model, &iter, DOCUMENT_LINKS_COLUMN_LINK, &link, -1);
			if (link && link->dest) {
				LinkDestination *dest = link->dest;

				if (dest->getPdfPage() == pdfPage) {
					// already bookmak from this page selected
					return true;
				}
			}
		}
	}

	gboolean valid = gtk_tree_model_iter_children(model, &iter, parent);

	while (valid) {
		XojLinkDest * link = NULL;

		gtk_tree_model_get(model, &iter, DOCUMENT_LINKS_COLUMN_LINK, &link, -1);

		if (link->dest->getPdfPage() == pdfPage) {
			GtkTreeSelection * selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeViewBookmarks));
			gtk_tree_selection_select_iter(selection, &iter);
			return true;
		} else {
			if (selectPageNr(page, pdfPage, &iter)) {
				return true;
			} else {
				valid = gtk_tree_model_iter_next(model, &iter);
			}
		}
	}
	return false;
}

PdfCache * Sidebar::getCache() {
	return this->cache;
}

void Sidebar::setTmpDisabled(bool disabled) {
	gtk_widget_set_sensitive(this->treeViewBookmarks, !disabled);
	gtk_widget_set_sensitive(this->iconViewPreview, !disabled);
	gtk_widget_set_sensitive(this->buttonCloseSidebar, !disabled);

	GdkCursor *cursor = NULL;

	if (disabled) {
		cursor = gdk_cursor_new(GDK_WATCH);
	}

	gdk_window_set_cursor(this->treeViewBookmarks->window, cursor);
	gdk_window_set_cursor(this->iconViewPreview->window, cursor);

	gdk_display_sync(gdk_display_get_default());

	if (cursor) {
		gdk_cursor_unref(cursor);
	}
}

void Sidebar::layout() {
	int x = 0;
	int y = 0;
	int width = 0;

	for (int i = 0; i < this->previewCount; i++) {
		SidebarPreview * p = this->previews[i];
		gtk_layout_move(GTK_LAYOUT(this->iconViewPreview), p->getWidget(), x, y);
		y += p->getHeight();
		width = MAX(width, p->getWidth());
	}

	gtk_layout_set_size(GTK_LAYOUT(iconViewPreview), width, y);
}

Document * Sidebar::getDocument() {
	return control->getDocument();
}

Control * Sidebar::getControl() {
	return control;
}

double Sidebar::getZoom() {
	return zoom;
}

void Sidebar::updatePreviews() {
	Document * doc = control->getDocument();
	int len = doc->getPageCount();

	if (this->previewCount == len) {
		return;
	}

	if (this->previews) {
		for (int i = 0; i < this->previewCount; i++) {
			delete this->previews[i];
		}
		delete[] this->previews;
	}

	this->previews = new SidebarPreview *[len];
	this->previewCount = len;

	for (int i = 0; i < len; i++) {
		SidebarPreview * p = new SidebarPreview(this, doc->getPage(i));
		this->previews[i] = p;
		gtk_layout_put(GTK_LAYOUT(this->iconViewPreview), p->getWidget(), 0, 0);
	}

	layout();
}

void Sidebar::documentChanged(DocumentChangeType type) {
	if (type == DOCUMENT_CHANGE_CLEARED) {
		gtk_tree_view_set_model(GTK_TREE_VIEW(treeViewBookmarks), NULL);
	} else if (type == DOCUMENT_CHANGE_PDF_BOOKMARKS || type == DOCUMENT_CHANGE_COMPLETE) {
		GtkTreeModel * model = control->getDocument()->getContentsModel();
		gtk_tree_view_set_model(GTK_TREE_VIEW(treeViewBookmarks), model);
		int count = expandOpenLinks(model, NULL);
		if (count == 0) {
			gtk_combo_box_set_active(GTK_COMBO_BOX(comboBox), 1);
		} else if (this->typeSelected) {
			gtk_combo_box_set_active(GTK_COMBO_BOX(comboBox), 0);
		}
	}

	if (type == DOCUMENT_CHANGE_COMPLETE || type == DOCUMENT_CHANGE_CLEARED) {
		updatePreviews();
	}
}

void Sidebar::pageSizeChanged(int page) {
	if (page < 0 || page >= this->previewCount) {
		return;
	}
	SidebarPreview * p = this->previews[page];
	p->updateSize();
	p->repaint();

	layout();
}

void Sidebar::pageChanged(int page) {
	if (page < 0 || page >= this->previewCount) {
		return;
	}

	SidebarPreview * p = this->previews[page];
	p->repaint();
}

bool Sidebar::scrollTopreview(Sidebar * sidebar) {
	if (sidebar->selectedPage >= 0 && sidebar->selectedPage < sidebar->previewCount) {
		gdk_threads_enter();
		SidebarPreview * p = sidebar->previews[sidebar->selectedPage];

		// scroll to preview
		GtkAdjustment * hadj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(sidebar->scrollPreview));
		GtkAdjustment * vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(sidebar->scrollPreview));
		GtkWidget * widget = p->getWidget();
		int x = widget->allocation.x;
		int y = widget->allocation.y;
		gdk_threads_leave();

		if (x == -1) {
			g_idle_add((GSourceFunc) &scrollTopreview, sidebar);
			return false;
		}

		gdk_threads_enter();
		gtk_adjustment_clamp_page(vadj, y, y + widget->allocation.height);
		gtk_adjustment_clamp_page(hadj, x, x + widget->allocation.width);
		gdk_threads_leave();
	}
	return false;
}

void Sidebar::pageDeleted(int page) {
	delete this->previews[page];
	for (int i = page; i < this->previewCount; i++) {
		this->previews[i] = this->previews[i + 1];
	}
	this->previewCount--;

	layout();
}

void Sidebar::pageInserted(int page) {
	SidebarPreview ** lastPreviews = this->previews;

	this->previews = new SidebarPreview *[this->previewCount + 1];

	for (int i = 0; i < page; i++) {
		this->previews[i] = lastPreviews[i];
	}

	for (int i = page; i < this->previewCount; i++) {
		this->previews[i + 1] = lastPreviews[i];
	}

	this->previewCount++;

	delete[] lastPreviews;

	Document * doc = control->getDocument();

	SidebarPreview * p = new SidebarPreview(this, doc->getPage(page));
	this->previews[page] = p;
	gtk_layout_put(GTK_LAYOUT(this->iconViewPreview), p->getWidget(), 0, 0);

	layout();
}

void Sidebar::pageSelected(int page) {
	if (this->selectedPage >= 0 && this->selectedPage < this->previewCount) {
		this->previews[this->selectedPage]->setSelected(false);
	}
	this->selectedPage = page;

	if (this->selectedPage >= 0 && this->selectedPage < this->previewCount) {
		SidebarPreview * p = this->previews[this->selectedPage];
		p->setSelected(true);
		scrollTopreview(this);
	}
}
