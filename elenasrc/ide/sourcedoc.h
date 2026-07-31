//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      SourceDoc class header
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef sourcedocH
#define sourcedocH

#include "document.h"

namespace _GUI_
{

// --- LexicalStyler ---

#define INDEX_STEP  0x100
#define INDEX_ORDER 8

class LexicalStylist
{
   MemoryDump _index;
   MemoryDump _lexic;

   int retrievePosition(size_t position)
   {
      position = (position >> INDEX_ORDER);

      DumpReader reader(&_index, position * 4);

      return reader.getDWord();
   }

public:
   void parse(Text* text);

   size_t proceed(LineInfo& info);

   LexicalStylist();
};

// --- SourceDoc ---

class SourceDoc : public Document
{
   LexicalStylist _stylist;

   bool      _tracking;
   TrackInfo _tracker;
   size_t    _trackLineStyle;
   size_t    _trackStyle;

   bool      _withBracketHighlighting;
   Point     _highlightedBrackets; // currently highlighted brackets positions (x - openning, y - closing), or -1 if not
   
   virtual void onChange();

   virtual size_t defineStyle(LineInfo& info);         

   bool findBracket(TextBookmark& bookmark, char starting, char ending, bool forward);
   virtual bool highlightCharacter();

public:
   virtual void setTracker(TrackInfo info, int lineStyle, int style);
   virtual void clearTracker();

   virtual void refresh();

   SourceDoc(Text* text, FileEncoding encoding);
};

} // _GUI_

#endif // sourcedocH
