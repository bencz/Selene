//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA Compiler
//
//		This file contains ELENA Engine Syntax Tree class implementation
//
//                                              (C)2005-2015, by Alexei Rakov
//---------------------------------------------------------------------------

#include "elena.h"
// --------------------------------------------------------------------------
#include "syntaxtree.h"
#include <stdarg.h>

using namespace _ELENA_;

// --- SyntaxWriter ---

void SyntaxWriter :: insert(int bookmark, LexicalType type, ref_t argument)
{
   size_t position = (bookmark == 0) ? _bookmarks.peek() : *_bookmarks.get(_bookmarks.Count() - bookmark);

   // NOTE : a tree record is two native words - [type][argument] - so the
   // argument can carry a pointer on 64-bit hosts; the dump is never persisted
   _writer.insertSize(position, (size_t)type);
   _writer.insertSize(position + sizeof(size_t), argument);

   Stack<size_t>::Iterator it = _bookmarks.start();
   while (!it.Eof()) {
      if (*it > position) {
         *it = *it + 2 * sizeof(size_t);
      }

      it++;
   }
}

void SyntaxWriter :: newNode(LexicalType type, ref_t argument)
{
   // writer node
   _writer.writeSize((size_t)type);
   _writer.writeSize(argument);
}

void SyntaxWriter :: closeNode()
{
   _writer.writeSize((size_t)-1);
   _writer.writeSize(0);
}

// --- SyntaxTree::Node ---

SyntaxTree::Node :: Node(SyntaxTree* tree, size_t position, LexicalType type, ref_t argument)
{
   this->tree = tree;
   this->position = position;

   this->type = type;
   this->argument = argument;
}

// --- SyntaxReader ---

SyntaxTree::Node SyntaxTree :: insertNode(size_t position, LexicalType type, ref_t argument)
{
   SyntaxWriter writer(_dump);

   writer.insertChild(writer.setBookmark(position), type, argument);

   _reader.seek(position);

   return read();
}

SyntaxTree::Node SyntaxTree:: read()
{
   size_t type = _reader.getSize();
   ref_t arg = _reader.getSize();

   if (type == (size_t)-1) {
      return Node();
   }
   else return Node(this, _reader.Position(), (LexicalType)type, arg);
}

SyntaxTree::Node SyntaxTree:: readRoot()
{
   _reader.seek(0);

   return read();
}

SyntaxTree::Node SyntaxTree:: readFirstNode(size_t position)
{
   _reader.seek(position);

   return read();
}

SyntaxTree::Node SyntaxTree:: readNextNode(size_t position)
{
   _reader.seek(position);

   int level = 1;

   do {
      size_t type = _reader.getSize();
      _reader.getSize();

      if (type == (size_t)-1) {
         level--;
      }
      else level++;

   } while (level > 0);

   return read();
}


SyntaxTree::Node SyntaxTree :: readPreviousNode(size_t position)
{
   const size_t recordSize = 2 * sizeof(size_t);

   position = position - 2 * recordSize;

   int level = 0;
   while (position > recordSize - 1) {
      _reader.seek(position);

      size_t type = _reader.getSize();
      _reader.getSize();

      if (type != (size_t)-1) {
         if (level == 0)
            break;

         level++;
         if (level == 0) {
            _reader.seek(position);

            return read();
         }
      }
      else level--;

      position -= recordSize;
   }

   return Node();
}

bool SyntaxTree :: matchPattern(Node node, int mask, int counter, ...)
{
   va_list argptr;
   va_start(argptr, counter);

   Node member = node.firstChild();
   if (member == lxNone)
      return false;

   for (int i = 0; i < counter; i++) {
      // get the next pattern
      NodePattern pattern = va_arg(argptr, NodePattern);

      // find the next tree node
      while (!test(member.type, mask)) {
         member = member.nextNode();
         if (member == lxNone) {
            va_end(argptr);
            return false;
         }
      }

      if (!pattern.match(member)) {
         va_end(argptr);
         return false;
      }
      else member = member.nextNode();
   }

   va_end(argptr);
   return true;
}

SyntaxTree::Node SyntaxTree :: findPattern(Node node, int counter, ...)
{
   va_list argptr;
   va_start(argptr, counter);

   Node member = node;
   for (int i = 0; i < counter; i++) {
      member = member.firstChild();
      if (member == lxNone)
         break;

      // get the next pattern
      NodePattern pattern = va_arg(argptr, NodePattern);

      // find the matched member
      while (member != lxNone && !pattern.match(member)) {
         member = member.nextNode();
      }
   }

   va_end(argptr);

   return member;
}
