/*
 * Xournal++
 *
 * An iterator over a GList
 *
 * @author Xournal Team
 * http://xournal.sf.net
 *
 * @license GPL
 */

#ifndef __LISTITERATOR_H__
#define __LISTITERATOR_H__

#include <gtk/gtk.h>

template<class T>
class ListIterator {
public:
	ListIterator(GList * data, bool reverse = false) {
		if (reverse) {
			this->data = g_list_last(data);
		} else {
			this->data = data;
		}
		this->reverse = reverse;
		this->copied = false;
	}

	virtual ~ListIterator() {
		if (this->copied) {
			g_list_free(this->data);
		}
		this->copied = false;
		this->data = NULL;
	}

	/**
	 * If the source changes while you are using the iterator nothing happens
	 */
	void freeze() {
		if (this->copied) {
			return;
		}
		this->copied = true;
		this->data = g_list_copy(this->data);
	}

	bool hasNext() {
		return this->data != NULL;
	}

	T next() {
		T d = (T) this->data->data;
		if (reverse) {
			this->data = this->data->prev;
		} else {
			this->data = this->data->next;
		}
		return d;
	}

private:
	GList * data;
	bool reverse;
	bool copied;
};

#endif /* __LISTITERATOR_H__ */