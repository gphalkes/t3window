/* Copyright (C) 2011,2018 G.P. Halkes
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 3, as
   published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef T3_WINDOW_H
#define T3_WINDOW_H

/** @defgroup t3window_win Window manipulation functions. */

#include <stdlib.h>
#include <t3window/terminal.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Define a parent anchor point for a relation (see t3_win_anchor_t). @ingroup t3window_other */
#define T3_PARENT(_x) ((_x) << 4)
/** Define a child anchor point a relation (see t3_win_anchor_t). @ingroup t3window_other */
#define T3_CHILD(_x) ((_x) << 8)
/** Get a parent anchor point from a relation (see t3_win_anchor_t). @ingroup t3window_other */
#define T3_GETPARENT(_x) (((_x) >> 4) & 0xf)
/** Get a child anchor point from a relation (see t3_win_anchor_t). @ingroup t3window_other */
#define T3_GETCHILD(_x) (((_x) >> 8) & 0xf)

/** Anchor points for defining relations between the positions of two windows.

    The anchor points can be used to define the relative positioning of two
    windows. For example, using T3_PARENT(T3_ANCHOR_TOPRIGHT) | T3_CHILD(T3_ANCHOR_TOPLEFT)
        allows positioning of one window left of another.

        @ingroup t3window_other
*/
enum t3_win_anchor_t {
  T3_ANCHOR_TOPLEFT,
  T3_ANCHOR_TOPRIGHT,
  T3_ANCHOR_BOTTOMLEFT,
  T3_ANCHOR_BOTTOMRIGHT,
  T3_ANCHOR_CENTER,
  T3_ANCHOR_TOPCENTER,
  T3_ANCHOR_BOTTOMCENTER,
  T3_ANCHOR_CENTERLEFT,
  T3_ANCHOR_CENTERRIGHT
};

/** An opaque struct representing a window which can be shown on the terminal.
    @ingroup t3window_other
*/
typedef struct t3_window_t t3_window_t;

T3_WINDOW_API t3_window_t *t3_win_new(t3_window_t *parent, int height, int width, int y, int x,
                                      int depth);
T3_WINDOW_API t3_window_t *t3_win_new_unbacked(t3_window_t *parent, int height, int width, int y,
                                               int x, int depth);
T3_WINDOW_API void t3_win_del(t3_window_t *win);

T3_WINDOW_API t3_bool t3_win_set_parent(t3_window_t *win, t3_window_t *parent);
T3_WINDOW_API t3_bool t3_win_set_anchor(t3_window_t *win, t3_window_t *anchor, int relation);
T3_WINDOW_API void t3_win_set_depth(t3_window_t *win, int depth);
T3_WINDOW_API void t3_win_set_default_attrs(t3_window_t *win, t3_attr_t attr);
T3_WINDOW_API t3_bool t3_win_set_restrict(t3_window_t *win, t3_window_t *restrict);

T3_WINDOW_API t3_bool t3_win_resize(t3_window_t *win, int height, int width);
T3_WINDOW_API void t3_win_move(t3_window_t *win, int y, int x);
T3_WINDOW_API int t3_win_get_width(const t3_window_t *win);
T3_WINDOW_API int t3_win_get_height(const t3_window_t *win);
T3_WINDOW_API int t3_win_get_x(const t3_window_t *win);
T3_WINDOW_API int t3_win_get_y(const t3_window_t *win);
T3_WINDOW_API int t3_win_get_abs_x(const t3_window_t *win);
T3_WINDOW_API int t3_win_get_abs_y(const t3_window_t *win);
T3_WINDOW_API int t3_win_get_depth(const t3_window_t *win);
T3_WINDOW_API int t3_win_get_relation(const t3_window_t *win, t3_window_t **anchor);
T3_WINDOW_API t3_window_t *t3_win_get_parent(const t3_window_t *win);
T3_WINDOW_API void t3_win_set_cursor(t3_window_t *win, int y, int x);
T3_WINDOW_API void t3_win_set_paint(t3_window_t *win, int y, int x);
T3_WINDOW_API void t3_win_show(t3_window_t *win);
T3_WINDOW_API void t3_win_hide(t3_window_t *win);

T3_WINDOW_API int t3_win_addnstr(t3_window_t *win, const char *str, size_t n, t3_attr_t attr);
T3_WINDOW_API int t3_win_addstr(t3_window_t *win, const char *str, t3_attr_t attr);
T3_WINDOW_API int t3_win_addch(t3_window_t *win, char c, t3_attr_t attr);

T3_WINDOW_API int t3_win_addnstrrep(t3_window_t *win, const char *str, size_t n, t3_attr_t attr,
                                    int rep);
T3_WINDOW_API int t3_win_addstrrep(t3_window_t *win, const char *str, t3_attr_t attr, int rep);
T3_WINDOW_API int t3_win_addchrep(t3_window_t *win, char c, t3_attr_t attr, int rep);

T3_WINDOW_API int t3_win_box(t3_window_t *win, int y, int x, int height, int width, t3_attr_t attr);

T3_WINDOW_API void t3_win_clrtoeol(t3_window_t *win);
T3_WINDOW_API void t3_win_clrtobot(t3_window_t *win);

T3_WINDOW_API t3_window_t *t3_win_at_location(int search_y, int search_x);
#ifdef __cplusplus
} /* extern "C" */

#include <cstddef>
#include <new>
#include <utility>

namespace t3window {
/** Wrapper class for t3_window_t, to allow C++ style access. */
class T3_WINDOW_API window_t {
 public:
  window_t(t3_window_t *window) : window_(window) {}

  /// Constructor which calls alloc or alloc_unbacked to immediately allocate a window.
  window_t(const window_t *parent, int height, int width, int y, int x, int depth,
           bool backed = true) {
    if (backed) {
      alloc(parent, height, width, y, x, depth);
    } else {
      alloc_unbacked(parent, height, width, y, x, depth);
    }
  }

  ~window_t() { t3_win_del(window_); }

#if __cplusplus >= 201103L
  /// The default constructor does not yet allocate a window.
  window_t() : window_(nullptr) {}

  window_t(window_t &&other) { *this = std::move(other); }
  window_t &operator=(window_t &&other) {
    t3_win_del(window_);
    window_ = other.window_;
    other.window_ = nullptr;
    return *this;
  }

  bool operator==(std::nullptr_t) const { return window_ == nullptr; }
  bool operator!=(std::nullptr_t) const { return window_ != nullptr; }
  explicit operator bool() const { return !!window_; }
#else
  window_t() : window_(NULL) {}

  operator bool() const { return !!window_; }
#endif
  bool operator!() const { return !window_; }

  bool operator==(t3_window_t *other) const { return window_ == other; }
  bool operator!=(t3_window_t *other) const { return window_ != other; }
  bool operator==(void *p) const { return window_ == p; }
  bool operator!=(void *p) const { return window_ != p; }

  t3_window_t *get() { return window_; }
  const t3_window_t *get() const { return window_; }
  t3_window_t *release() {
    t3_window_t *result = window_;
#if __cplusplus >= 201103L
    window_ = nullptr;
#else
    window_ = NULL;
#endif
    return result;
  }

  void reset(t3_window_t *window) {
    if (window_) {
      t3_win_del(window_);
    }
    window_ = window;
  }

  /// Call t3_win_new. As new is a reserved keyword, this has been renamed to alloc.
  void alloc(const window_t *parent, int height, int width, int y, int x, int depth) {
    t3_win_del(window_);
    window_ = t3_win_new(parent == nullptr ? nullptr : parent->window_, height, width, y, x, depth);
    if (window_ == nullptr) {
      throw std::bad_alloc();
    }
  }

  /** Call t3_win_new_unbacked. To preserve consistency with t3_win_new->alloc, this is named
      alloc_unbacked. */
  void alloc_unbacked(const window_t *parent, int height, int width, int y, int x, int depth) {
    t3_win_del(window_);
    window_ = t3_win_new_unbacked(parent == nullptr ? nullptr : parent->window_, height, width, y,
                                  x, depth);
    if (window_ == nullptr) {
      throw std::bad_alloc();
    }
  }

  bool set_parent(const window_t *parent) const {
    return t3_win_set_parent(window_, parent == nullptr ? nullptr : parent->window_) != t3_false;
  }
  bool set_anchor(const window_t *anchor, int relation) {
    return t3_win_set_anchor(window_, anchor == nullptr ? nullptr : anchor->window_, relation) !=
           t3_false;
  }
  void set_depth(int depth) { t3_win_set_depth(window_, depth); }
  void set_default_attrs(t3_attr_t attr) { t3_win_set_default_attrs(window_, attr); }
  bool set_restrict(const window_t *other) {
    return t3_win_set_restrict(window_, other == nullptr ? nullptr : other->window_) != t3_false;
  }
  bool resize(int height, int width) { return t3_win_resize(window_, height, width) != t3_false; }
  void move(int y, int x) { t3_win_move(window_, y, x); }
  int get_width() const { return t3_win_get_width(window_); }
  int get_height() const { return t3_win_get_height(window_); }
  int get_x() const { return t3_win_get_x(window_); }
  int get_y() const { return t3_win_get_y(window_); }
  int get_abs_x() const { return t3_win_get_abs_x(window_); }
  int get_abs_y() const { return t3_win_get_abs_y(window_); }
  int get_depth() const { return t3_win_get_depth(window_); }
  int get_relation(t3_window_t **anchor) const { return t3_win_get_relation(window_, anchor); }
  t3_window_t *get_parent() const { return t3_win_get_parent(window_); }
  void set_cursor(int y, int x) { t3_win_set_cursor(window_, y, x); }
  void set_paint(int y, int x) { t3_win_set_paint(window_, y, x); }
  void show() { t3_win_show(window_); }
  void hide() { t3_win_hide(window_); }
  int addnstr(const char *str, size_t size, t3_attr_t attr) {
    return t3_win_addnstr(window_, str, size, attr);
  }
  int addstr(const char *str, t3_attr_t attr) { return t3_win_addstr(window_, str, attr); }
  int addch(char ch, t3_attr_t attr) { return t3_win_addch(window_, ch, attr); }
  int addnstrrep(const char *str, size_t size, t3_attr_t attr, int rep) {
    return t3_win_addnstrrep(window_, str, size, attr, rep);
  }
  int addstrrep(const char *str, t3_attr_t attr, int rep) {
    return t3_win_addstrrep(window_, str, attr, rep);
  }
  int addchrep(char ch, t3_attr_t attr, int rep) { return t3_win_addchrep(window_, ch, attr, rep); }
  int box(int y, int x, int height, int width, t3_attr_t attr) {
    return t3_win_box(window_, y, x, height, width, attr);
  }
  void clrtoeol() { t3_win_clrtoeol(window_); }
  void clrtobot() { t3_win_clrtobot(window_); }

 private:
#if __cplusplus >= 201103L
  window_t(const window_t &other) = delete;
  window_t &operator=(const window_t &other) = delete;
#else
  window_t(const window_t &other) {}
  window_t &operator=(const window_t &other) { return *this; }
#endif

  t3_window_t *window_;
};

inline bool operator==(t3_window_t *a, const window_t &b) { return b == a; }
inline bool operator!=(t3_window_t *a, const window_t &b) { return b != a; }
#if __cplusplus >= 201103L
inline bool operator==(std::nullptr_t, const window_t &b) { return b == nullptr; }
inline bool operator!=(std::nullptr_t, const window_t &b) { return b != nullptr; }
#else
inline bool operator==(void *p, const window_t &b) { return b == p; }
inline bool operator!=(void *p, const window_t &b) { return b != p; }
#endif

}  // namespace t3window
#endif

#endif
