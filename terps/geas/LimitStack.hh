/***************************************************************************
 *                                                                         *
 * Copyright (C) 2006 by Mark J. Tilford                                   *
 *                                                                         *
 * This file is part of Geas.                                              *
 *                                                                         *
 * Geas is free software; you can redistribute it and/or modify            *
 * it under the terms of the GNU General Public License as published by    *
 * the Free Software Foundation; either version 2 of the License, or       *
 * (at your option) any later version.                                     *
 *                                                                         *
 * Geas is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License       *
 * along with Geas; if not, write to the Free Software                     *
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *                                                                         *
 ***************************************************************************/

#ifndef __limitstack_hh
#define __limitstack_hh

#include <vector>
#include <iostream>
#include "general.hh"

template <class T> class LimitStack {
  size_t stack_size, cur_ptr, end_ptr;
  std::vector<T> data;
  //bool last_push;


  size_t dofwd (size_t i) const { i ++; return i == stack_size ? 0 : i; }
  size_t dobwd (size_t i) const { return (i == 0 ? stack_size : i) - 1; }
  void fwd (size_t &i) const { i = dofwd(i); }
  void bwd (size_t &i) const { i = dobwd(i); }

  /*
  void fwd (uint &i) { i ++; if (i == stack_size) i = 0; }
  void bwd (uint &i) { i = (i == 0 ? stack_size : i) - 1; }
  uint dofwd (uint i) { uint rv = i; fwd(rv); return rv; }
  uint dobwd (uint i) { uint rv = i; bwd(rv); return rv; }
  */

 public:
  LimitStack(uint size) : stack_size (size), cur_ptr (0), end_ptr (size - 1), data (std::vector<T> (size)) { }

  void push (T &item) 
  {
    if (cur_ptr == end_ptr)
      fwd (end_ptr);
    data[cur_ptr] = item;
    fwd (cur_ptr);
  }

  T &pop() 
  { 
    assert (!is_empty());
    bwd (cur_ptr);
    return data[cur_ptr];
  }

  bool is_empty() { return dobwd (cur_ptr) == end_ptr; }

  size_t size()
  { 
    if (cur_ptr > end_ptr)
      return cur_ptr - end_ptr - 1; 
    else 
      return (cur_ptr + stack_size) - end_ptr - 1; 
  }

  void dump (std::ostream &o) 
  {
    o << size() << ": < "; 
    for (uint i = dobwd(cur_ptr); i != end_ptr; bwd(i)) 
      o << data[i] << " "; 
    o << ">"; 
  }

  T &peek()
  {
    return data[dobwd(cur_ptr)];
  }
};

template<class T> std::ostream &operator<< (std::ostream &o, LimitStack<T> st) { st.dump(o); return o; }

#endif
